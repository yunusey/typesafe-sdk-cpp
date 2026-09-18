#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "mock_server.hpp"
#include "support/fixtures.hpp"
#include "typesafe/typesafe.hpp"

using namespace typesafe;          // NOLINT(build/namespaces)
using namespace typesafe::testing; // NOLINT(build/namespaces)
namespace fixtures = typesafe::examples::fixtures;

namespace
{

struct MockRuntime
{
    std::unique_ptr<MockServer> server;
    Client client;
};

MockRuntime mock_client(Json body)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .and_header("authorization", "Bearer mock-key")
        .respond_with(ResponseTemplate(200).set_body_json(body))
        .mount();
    auto client = Client::builder().api_key("mock-key").base_url(server->uri()).retry(RetryPolicy::disabled()).build();
    EXPECT_TRUE(client.has_value());
    return MockRuntime{std::move(server), std::move(client.value())};
}

} // namespace

TEST(Examples, SystemOne)
{
    auto rt = mock_client(fixtures::mixed_primitives());
    Json state = {{"document", "charged twice"}};
    auto response = rt.client.system_one(
        state, {
                   {"billing", Question::noul("billing?")},
                   {"tone", Question::choice("tone?", {{"calm", std::nullopt}, {"frustrated", std::nullopt}})},
                   {"urgency", Question::score("urgency?", {"wait", "week", "today"})},
               });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->noul("billing")->noul, 0.98);
    EXPECT_EQ(response->choice("tone")->choice, "frustrated");
    EXPECT_DOUBLE_EQ(response->score("urgency")->score, 2.2);
}

TEST(Examples, FanOut)
{
    auto rt = mock_client(fixtures::fan_out_triage());
    auto response = rt.client.system_one(
        "ticket", {
                      {"category", Question::choice("?", {{"bug_report", std::nullopt}, {"billing", std::nullopt}})},
                      {"has_reproducible_steps", Question::noul("?")},
                      {"frustration", Question::score("?", {"calm", "frustrated", "angry"})},
                  });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->choice("category")->choice, "bug_report");
    EXPECT_DOUBLE_EQ(response->noul("has_reproducible_steps")->noul, 0.82);
    EXPECT_DOUBLE_EQ(response->score("frustration")->score, 1.6);
}

TEST(Examples, ConfidenceRouting)
{
    auto rt = mock_client(fixtures::confidence_routing());
    auto response = rt.client.system_one(
        "msg", {{"action", Question::choice("?", {{"approve_transfer", std::nullopt}, {"support", std::nullopt}})}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    auto action = response->choice("action").value();
    EXPECT_EQ(action.choice, "approve_transfer");
    EXPECT_DOUBLE_EQ(action.confidence, 0.62);
}

TEST(Examples, Guardrails)
{
    auto rt = mock_client(fixtures::guardrails());
    auto response =
        rt.client.system_one("msg", {
                                        {"jailbreak", Question::noul("?")},
                                        {"harm_severity", Question::score("?", {"none", "moderate", "severe"})},
                                    });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->noul("jailbreak")->noul, 0.91);
    EXPECT_DOUBLE_EQ(response->score("harm_severity")->score, 2.4);
}

TEST(Examples, CitationCheck)
{
    auto rt = mock_client(fixtures::citation_check());
    Json state = {{"source", "policy"}, {"claim", "same day refund"}};
    auto response = rt.client.system_one(
        state, {{"supports_claim",
                 Question::choice("?", {{"yes", std::nullopt}, {"partially", std::nullopt}, {"no", std::nullopt}})}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    auto answer = response->choice("supports_claim").value();
    EXPECT_EQ(answer.choice, "partially");
    EXPECT_DOUBLE_EQ(answer.confidence, 0.45);
}

TEST(Examples, StructuredState)
{
    auto rt = mock_client(fixtures::structured_ticket());
    Json state = Json::parse(R"({"order": {"charges": [1, 2]}})");
    auto response = rt.client.system_one(state, {
                                                    {"duplicate_charge", Question::noul("?")},
                                                    {"refund_eligible", Question::noul("?")},
                                                });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->noul("duplicate_charge")->noul, 0.97);
    EXPECT_DOUBLE_EQ(response->noul("refund_eligible")->noul, 0.88);
}

TEST(Examples, CompositeScoring)
{
    auto rt = mock_client(fixtures::composite_scores());
    auto response =
        rt.client.system_one("draft", {
                                          {"clarity", Question::score("?", {"poor", "ok", "excellent"})},
                                          {"completeness", Question::score("?", {"missing", "partial", "complete"})},
                                          {"tone", Question::score("?", {"harsh", "neutral", "warm"})},
                                      });
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->score("clarity")->score, 2.0);
    EXPECT_DOUBLE_EQ(response->score("completeness")->score, 1.2);
    EXPECT_DOUBLE_EQ(response->score("tone")->score, 1.8);
}

TEST(Examples, IntentRouting)
{
    auto rt = mock_client(fixtures::intent_routing());
    auto response =
        rt.client.system_one("summarize revenue", {{"intent", Question::choice("?", {{"deterministic", std::nullopt},
                                                                                     {"specialist_llm", std::nullopt},
                                                                                     {"human", std::nullopt}})}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    auto intent = response->choice("intent").value();
    EXPECT_EQ(intent.choice, "specialist_llm");
    EXPECT_DOUBLE_EQ(intent.confidence, 0.79);
}

TEST(Examples, NoulUncertainty)
{
    auto rt = mock_client(fixtures::noul_uncertainty_band());
    auto response = rt.client.system_one("claim", {{"fraud_likely", Question::noul("?")}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->noul("fraud_likely")->noul, 0.48);
}

TEST(Examples, BlockingTriage)
{
    auto rt = mock_client(fixtures::fan_out_triage());
    auto response = rt.client.system_one("Billing double-charged my card.",
                                         {{"category", Question::choice("?", {{"bug_report", std::nullopt},
                                                                              {"billing", std::nullopt},
                                                                              {"feature_request", std::nullopt}})}});
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->choice("category")->choice, "bug_report");
}
