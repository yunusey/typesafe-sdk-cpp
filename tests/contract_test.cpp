#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

#include "mock_server.hpp"
#include "typesafe/typesafe.hpp"

using namespace typesafe;          // NOLINT(build/namespaces)
using namespace typesafe::testing; // NOLINT(build/namespaces)

namespace
{

Json result_body()
{
    return Json::parse(R"({
    "model": "jev-latest",
    "usage": {"input_tokens": 12, "output_tokens": 3},
    "answers": {
      "spam": {"type": "noul", "noul": 0.98},
      "tone": {
        "type": "choice",
        "choice": "friendly",
        "confidence": 0.9,
        "probabilities": {"friendly": 0.9, "hostile": 0.1}
      },
      "quality": {
        "type": "score",
        "score": 1.7,
        "confidence": 0.8,
        "legend": {"0": "bad", "1": "ok", "2": "great"},
        "probabilities": {"0": 0.1, "1": 0.1, "2": 0.8}
      }
    }
  })");
}

Client make_client(const MockServer &server)
{
    auto client = Client::builder().api_key("test-key").base_url(server.uri()).retry(RetryPolicy::disabled()).build();
    EXPECT_TRUE(client.has_value()) << (client ? "" : client.error().to_string());
    return std::move(client.value());
}

} // namespace

TEST(Contract, SystemOneRoundTrip)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .and_header("authorization", "Bearer test-key")
        .and_header("accept", "application/json")
        .respond_with(ResponseTemplate(200).set_body_json(result_body()))
        .mount();

    Client client = make_client(*server);
    Json state = {{"document", "Hello \U0001F30D"}};
    auto response = client.system_one(
        state, {
                   {"spam", Question::noul("Spam?")},
                   {"tone", Question::choice("Tone?", {{"friendly", std::nullopt}, {"hostile", std::nullopt}})},
                   {"quality", Question::score("Quality?", {"bad", "ok", "great"})},
               });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();

    auto received = server->received_requests();
    ASSERT_EQ(received.size(), 1U);
    Json body = Json::parse(received[0].body);
    Json expected = Json::parse(R"({
    "state": {"document": "Hello \ud83c\udf0d"},
    "model": "jev-latest",
    "questions": {
      "spam": {"type": "noul", "instructions": "Spam?"},
      "tone": {"type": "choice", "instructions": "Tone?", "criteria": {"friendly": null, "hostile": null}},
      "quality": {"type": "score", "instructions": "Quality?", "criteria": ["bad", "ok", "great"]}
    }
  })");
    EXPECT_EQ(body, expected);

    EXPECT_EQ(response->model, "jev-latest");
    EXPECT_EQ(response->usage.input_tokens, std::optional<std::int64_t>(12));
    EXPECT_EQ(response->usage.output_tokens, std::optional<std::int64_t>(3));
    EXPECT_DOUBLE_EQ(response->noul("spam")->noul, 0.98);
    EXPECT_EQ(response->choice("tone")->choice, "friendly");
    EXPECT_DOUBLE_EQ(response->choice("tone")->confidence, 0.9);
    EXPECT_DOUBLE_EQ(response->score("quality")->score, 1.7);
    EXPECT_EQ(response->score("quality")->legend.at(0), Json("bad"));
    EXPECT_DOUBLE_EQ(response->score("quality")->probabilities.at(2), 0.8);

    EXPECT_FALSE(received[0].header("x-typesafe-retry-count").has_value());
    ASSERT_TRUE(received[0].header("user-agent").has_value());
    EXPECT_TRUE(received[0].header("user-agent")->starts_with("typesafe-sdk/"));
    ASSERT_TRUE(received[0].header("x-typesafe-runtime").has_value());
    EXPECT_NE(received[0].header("x-typesafe-runtime")->find('('), std::string::npos);
}

TEST(Contract, ExtraBodyOverridesModel)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone").respond_with(ResponseTemplate(200).set_body_json(result_body())).mount();

    Client client = make_client(*server);
    Json extra = {{"model", "override-model"}, {"beam_width", 4}, {"nullable", nullptr}};
    SystemOneOpts opts;
    opts.model = "call-model";
    opts.extra_body = extra;
    auto response = client.system_one_opts("hi", {{"q", Question::noul("?")}}, opts);
    ASSERT_TRUE(response.has_value()) << response.error().to_string();

    Json body = Json::parse(server->received_requests()[0].body);
    Json expected = Json::parse(R"({
    "state": "hi",
    "model": "override-model",
    "questions": {"q": {"type": "noul", "instructions": "?"}},
    "beam_width": 4,
    "nullable": null
  })");
    EXPECT_EQ(body, expected);
}

TEST(Contract, EmptyQuestionsNeverHitTheNetwork)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone").respond_with(ResponseTemplate(500)).mount();

    Client client = make_client(*server);
    auto error = client.system_one("x", {});
    ASSERT_FALSE(error.has_value());
    EXPECT_NE(error.error().to_string().find("At least one question"), std::string::npos);
    EXPECT_TRUE(server->received_requests().empty());
}

TEST(Contract, EmptyScoreCriteriaNeverHitTheNetwork)
{
    auto server = MockServer::start();
    Client client = make_client(*server);
    auto error = client.system_one("x", {{"rating", Question::score("?", {})}});
    ASSERT_FALSE(error.has_value());
    EXPECT_NE(error.error().to_string().find("\"rating\" has no criteria"), std::string::npos);
}

TEST(Contract, RawQuestionPassthrough)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone").respond_with(ResponseTemplate(200).set_body_json(result_body())).mount();

    Client client = make_client(*server);
    Json raw = {{"type", "noul"}, {"instructions", "Spam?"}, {"weight", 3}};
    auto response = client.system_one("hi", {{"q", Question::raw(raw)}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();

    Json body = Json::parse(server->received_requests()[0].body);
    EXPECT_EQ(body["questions"]["q"]["weight"], 3);
}

TEST(Contract, UnknownAnswerTypeIsSkipped)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .respond_with(ResponseTemplate(200).set_body_json(Json::parse(R"({
        "model": "test",
        "usage": {"input_tokens": 1, "output_tokens": 1},
        "answers": {
          "spam": {"type": "noul", "noul": 0.9},
          "mystery": {"type": "aurora", "value": 3}
        }
      })")))
        .mount();

    Client client = make_client(*server);
    auto response = client.system_one("text", {{"q", Question::noul("?")}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->answers.size(), 1U);
    EXPECT_DOUBLE_EQ(response->noul("spam")->noul, 0.9);
    Json raw = Json::parse(response->raw_body());
    EXPECT_EQ(raw["answers"]["mystery"]["type"], "aurora");
}

TEST(Contract, MalformedResponseNamesTheField)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .respond_with(
            ResponseTemplate(200).insert_header("x-typesafe-request-id", "req-123").set_body_json(Json::parse(R"({
                          "model": "test",
                          "usage": {"input_tokens": 1, "output_tokens": 1},
                          "answers": {"n": {"type": "noul"}}
                        })")))
        .mount();

    Client client = make_client(*server);
    auto error = client.system_one("x", {{"q", Question::noul("?")}});
    ASSERT_FALSE(error.has_value());
    const ApiError *api = error.error().api();
    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->kind, ApiErrorKind::ResponseValidation);
    EXPECT_EQ(api->field_path, std::optional<std::string>("answers.n.noul"));
    EXPECT_EQ(api->request_id(), std::optional<std::string>("req-123"));
}

TEST(Contract, MalformedResponseToStringExact)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .respond_with(
            ResponseTemplate(200).insert_header("x-typesafe-request-id", "req-123").set_body_json(Json::parse(R"({
                          "model": "test",
                          "usage": {"input_tokens": 1, "output_tokens": 1},
                          "answers": {"n": {"type": "noul"}}
                        })")))
        .mount();

    Client client = make_client(*server);
    auto error = client.system_one("x", {{"q", Question::noul("?")}});
    ASSERT_FALSE(error.has_value());
    std::string expected = "POST " + server->uri() +
                           "/v1/systemone: 200 Invalid response data at 'answers.n.noul'. "
                           "(request_id=req-123)";
    EXPECT_EQ(error.error().to_string(), expected);
}

TEST(Contract, ErrorStatusMapping)
{
    auto server = MockServer::start();
    server->given("GET", "/v1/models")
        .respond_with(ResponseTemplate(429)
                          .insert_header("x-typesafe-request-id", "req_123")
                          .insert_header("retry-after-ms", "125")
                          .set_body_json(Json::parse(R"({"detail": {"message": "Server explanation"}})")))
        .mount();

    Client client = make_client(*server);
    auto error = client.models();
    ASSERT_FALSE(error.has_value());
    const ApiError *api = error.error().api();
    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->kind, ApiErrorKind::RateLimited);
    EXPECT_EQ(api->status, 429);
    ASSERT_TRUE(api->retry_after_ms().has_value());
    EXPECT_DOUBLE_EQ(std::round(*api->retry_after_ms()), 125.0);
    EXPECT_NE(error.error().to_string().find("429 Server explanation"), std::string::npos);
    EXPECT_NE(error.error().to_string().find("request_id=req_123"), std::string::npos);
}

TEST(Contract, ModelsList)
{
    auto server = MockServer::start();
    server->given("GET", "/v1/models")
        .respond_with(ResponseTemplate(200).set_body_json(Json::parse(R"({
        "models": [{
          "name": "jev-latest",
          "description": "Fast model",
          "release_date": "2026-08-01",
          "context_window": 128000
        }]
      })")))
        .mount();

    Client client = make_client(*server);
    auto response = client.models();
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->models[0].name, "jev-latest");
    EXPECT_EQ(response->models[0].description, "Fast model");
    Json raw = Json::parse(response->raw_body());
    EXPECT_EQ(raw["models"][0]["context_window"], 128000);
}

TEST(Contract, RetriesThenSucceeds)
{
    auto server = MockServer::start();
    server->given("GET", "/v1/models")
        .respond_with(ResponseTemplate(503)
                          .insert_header("retry-after-ms", "0")
                          .set_body_json(Json::parse(R"({"message": "temporarily unavailable"})")))
        .up_to_n_times(2)
        .mount();
    server->given("GET", "/v1/models")
        .respond_with(ResponseTemplate(200).set_body_json(Json::parse(R"({"models": []})")))
        .mount();

    RetryPolicy policy;
    policy.backoff_initial = std::chrono::duration<double>(0.0);
    policy.backoff_max = std::chrono::duration<double>(0.0);
    policy.timeout = std::nullopt;
    auto client = Client::builder().api_key("test-key").base_url(server->uri()).retry(policy).build();
    ASSERT_TRUE(client.has_value());

    auto response = client->models();
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_TRUE(response->models.empty());

    auto requests = server->received_requests();
    ASSERT_EQ(requests.size(), 3U);
    EXPECT_FALSE(requests[0].header("x-typesafe-retry-count").has_value());
    EXPECT_EQ(requests[1].header("x-typesafe-retry-count"), std::optional<std::string>("1"));
    EXPECT_EQ(requests[2].header("x-typesafe-retry-count"), std::optional<std::string>("2"));
}

TEST(Contract, NoulCriteriaAndRichJson)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .respond_with(ResponseTemplate(200).set_body_json(Json::parse(R"({
        "model": "custom",
        "usage": {"input_tokens": 1, "output_tokens": 1},
        "answers": {
          "risk": {
            "type": "score",
            "score": 0.0,
            "confidence": 1.0,
            "legend": {"0": {"summary": "duplicated", "examples": ["charged twice"]}},
            "probabilities": {"0": 1.0}
          }
        }
      })")))
        .mount();

    Client client = make_client(*server);
    Json criteria = Json::parse(R"({"summary": "duplicated", "examples": ["charged twice"]})");

    SystemOneOpts opts;
    opts.model = "custom";
    auto response =
        client.system_one_opts("a ticket",
                               {
                                   {"duplicate", Question::noul(Json::parse(R"({"question": "Duplicate?"})"))
                                                     .with_noul_criteria(NoulCriteria{}.yes(criteria))},
                                   {"risk", Question::score("Risk?", {criteria})},
                               },
                               opts);
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->score("risk")->legend.at(0), criteria);
}

TEST(Contract, MissingApiKey)
{
    ::unsetenv("TYPESAFE_API_KEY");
    auto error = Client::from_env();
    ASSERT_FALSE(error.has_value());
    EXPECT_NE(error.error().to_string().find("TYPESAFE_API_KEY"), std::string::npos);
}

TEST(Contract, ZeroTimeoutIsRejected)
{
    auto error = Client::builder().api_key("k").timeout(std::chrono::duration<double>(0.0)).build();
    ASSERT_FALSE(error.has_value());
    EXPECT_NE(error.error().to_string().find("timeout"), std::string::npos);
}
