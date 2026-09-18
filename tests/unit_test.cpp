#include <gtest/gtest.h>

#include <set>

#include "logging.hpp" // internal header (src/)
#include "typesafe/error.hpp"
#include "typesafe/json.hpp"
#include "typesafe/question.hpp"
#include "typesafe/retry.hpp"

using namespace typesafe; // NOLINT(build/namespaces)

TEST(Backoff, MatchesPythonTable)
{
    struct Case
    {
        std::uint32_t attempt;
        double seconds;
    };
    for (Case c : {Case{1, 0.5}, Case{2, 1.0}, Case{3, 2.0}, Case{4, 4.0}, Case{5, 5.0}, Case{20, 5.0}})
    {
        EXPECT_DOUBLE_EQ(detail::backoff(c.attempt, 0.5, 5.0, 0.25, 0.0).count(), c.seconds) << "attempt " << c.attempt;
    }
    EXPECT_DOUBLE_EQ(detail::backoff(1, 0.5, 5.0, 0.25, 1.0).count(), 0.375);
}

TEST(Backoff, ZeroDisablesDelay)
{
    EXPECT_DOUBLE_EQ(detail::backoff(3, 0.0, 5.0, 0.25, 0.0).count(), 0.0);
    EXPECT_DOUBLE_EQ(detail::backoff(3, 0.5, 0.0, 0.25, 0.0).count(), 0.0);
}

TEST(RetryStatuses, DefaultMatchesPythonSet)
{
    auto statuses = RetryStatuses::default_set();
    for (int status : {408, 429, 500, 503, 599})
    {
        EXPECT_TRUE(statuses.contains(status)) << status;
    }
    for (int status : {400, 404, 409, 499})
    {
        EXPECT_FALSE(statuses.contains(status)) << status;
    }
}

TEST(RetryStatuses, CustomIsExact)
{
    auto statuses = RetryStatuses::custom({409});
    EXPECT_TRUE(statuses.contains(409));
    EXPECT_FALSE(statuses.contains(429));
    EXPECT_FALSE(RetryStatuses::custom({}).contains(503));
}

TEST(Error, ExtractMessageOrder)
{
    EXPECT_EQ(detail::extract_message(Json::parse(R"({"error": "error", "message": "message", "detail": "detail"})")),
              std::optional<std::string>("error"));
    EXPECT_EQ(detail::extract_message(Json::parse(R"({"error": {"message": "nested error"}, "message": "message"})")),
              std::optional<std::string>("nested error"));
    EXPECT_EQ(detail::extract_message(Json::parse(R"({"message": "message", "detail": "detail"})")),
              std::optional<std::string>("message"));
    EXPECT_EQ(detail::extract_message(Json::parse(R"({"detail": "detail"})")), std::optional<std::string>("detail"));
    EXPECT_EQ(detail::extract_message(Json::parse(R"({"detail": {"message": "nested detail"}})")),
              std::optional<std::string>("nested detail"));
    EXPECT_EQ(detail::extract_message(Json::parse(R"({
        "detail": [
          {"loc": ["body", "questions", "q", "score", "criteria", 0], "msg": "Invalid"},
          {"msg": "Missing"},
          {}
        ]
      })")),
              std::optional<std::string>("questions.q.score.criteria.0: Invalid; Missing"));
}

TEST(Error, EndpointDropsUserinfoAndQuery)
{
    EXPECT_EQ(detail::format_endpoint("GET", "https://user:password@example.test/v1/models?token=secret#fragment"),
              "GET https://example.test/v1/models");
}

TEST(Question, TypedQuestionsOmitDefaultFields)
{
    auto noul = Question::noul_bare().to_wire("q");
    ASSERT_TRUE(noul.has_value());
    EXPECT_EQ(*noul, Json::parse(R"({"type": "noul"})"));

    auto choice = Question::choice("Tone?", {{"calm", std::nullopt}}).to_wire("q");
    ASSERT_TRUE(choice.has_value());
    EXPECT_EQ(*choice, Json::parse(R"({"type": "choice", "instructions": "Tone?", "criteria": {"calm": null}})"));
}

TEST(Question, RawScoreWithoutCriteriaFails)
{
    Json raw = Json::parse(R"({"type": "score", "instructions": "?"})");
    auto error = Question::raw(raw).to_wire("rating");
    ASSERT_FALSE(error.has_value());
    EXPECT_NE(error.error().to_string().find("requires \"criteria\""), std::string::npos);
}

TEST(Json, StateRejectsScalars)
{
    EXPECT_TRUE(to_state(Json("text")).has_value());
    EXPECT_TRUE(to_state(Json::parse(R"({"a": 1})")).has_value());
    EXPECT_TRUE(to_state(Json::parse(R"([1, 2])")).has_value());
    EXPECT_FALSE(to_state(Json(nullptr)).has_value());
    EXPECT_FALSE(to_state(Json(true)).has_value());
    EXPECT_FALSE(to_state(Json(3)).has_value());
}

TEST(Logging, RedactsAuthorization)
{
    Headers headers;
    headers.set("authorization", "Bearer secret");
    headers.set("accept", "application/json");
    auto redacted = detail::redact(headers);
    bool auth_hidden = false;
    bool accept_shown = false;
    for (const auto &[name, value] : redacted)
    {
        if (name == "authorization" && value == "***")
        {
            auth_hidden = true;
        }
        if (name == "accept" && value == "application/json")
        {
            accept_shown = true;
        }
    }
    EXPECT_TRUE(auth_hidden);
    EXPECT_TRUE(accept_shown);
}
