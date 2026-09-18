#include <gtest/gtest.h>

#include <typesafe/coro.hpp>

#include "mock_server.hpp"
#include "support/fixtures.hpp"

using namespace typesafe;          // NOLINT(build/namespaces)
using namespace typesafe::testing; // NOLINT(build/namespaces)
namespace coro = typesafe::coro;
namespace fixtures = typesafe::examples::fixtures;

namespace
{

std::unique_ptr<MockServer> make_server()
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .and_header("authorization", "Bearer mock-key")
        .respond_with(ResponseTemplate(200).set_body_json(fixtures::mixed_primitives()))
        .mount();
    return server;
}

Client make_client(const MockServer &server)
{
    auto client = Client::builder().api_key("mock-key").base_url(server.uri()).retry(RetryPolicy::disabled()).build();
    EXPECT_TRUE(client.has_value());
    return std::move(client.value());
}

} // namespace

TEST(Async, SystemOneWithThreadPool)
{
    auto server = make_server();
    Client client = make_client(*server);
    coro::ThreadPoolExecutor scheduler{2};

    auto response =
        coro::sync_wait(coro::system_one(scheduler, client, "hi", {{"billing", Question::noul("billing?")}}));
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_DOUBLE_EQ(response->noul("billing")->noul, 0.98);
}

TEST(Async, InlineExecutorRunsSynchronously)
{
    auto server = make_server();
    Client client = make_client(*server);
    coro::InlineExecutor scheduler;

    auto response =
        coro::sync_wait(coro::system_one(scheduler, client, "hi", {{"billing", Question::noul("billing?")}}));
    ASSERT_TRUE(response.has_value()) << response.error().to_string();
    EXPECT_EQ(response->choice("tone")->choice, "frustrated");
}

TEST(Async, ConcurrentFanOut)
{
    auto server = MockServer::start();
    server->given("POST", "/v1/systemone")
        .respond_with(ResponseTemplate(200).set_body_json(fixtures::mixed_primitives()))
        .mount();
    Client client = make_client(*server);
    coro::ThreadPoolExecutor scheduler{4};

    auto combined = [&]() -> coro::Task<int> {
        auto a = coro::system_one(scheduler, client, "a", {{"billing", Question::noul("?")}});
        auto b = coro::system_one(scheduler, client, "b", {{"billing", Question::noul("?")}});
        auto c = coro::system_one(scheduler, client, "c", {{"billing", Question::noul("?")}});
        auto ra = co_await a;
        auto rb = co_await b;
        auto rc = co_await c;
        int ok = 0;
        ok += ra && ra->noul("billing")->noul == 0.98 ? 1 : 0;
        ok += rb && rb->noul("billing")->noul == 0.98 ? 1 : 0;
        ok += rc && rc->noul("billing")->noul == 0.98 ? 1 : 0;
        co_return ok;
    };

    EXPECT_EQ(coro::sync_wait(combined()), 3);
    EXPECT_EQ(server->received_requests().size(), 3U);
}
