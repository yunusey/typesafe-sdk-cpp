#include <chrono>
#include <iostream>

#include <typesafe/coro.hpp>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace coro = typesafe::coro;
namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Client;
using typesafe::Question;
using typesafe::SystemOneResponse;
using Answer = typesafe::Result<SystemOneResponse>;

namespace
{

template <coro::Scheduler S> coro::Task<int> run(S &scheduler, const Client &client)
{
    const char *ticket = "I was double charged and I am furious. Fix it today.";

    auto billing = coro::system_one(scheduler, client, ticket, {{"billing", Question::noul("Is this about billing?")}});
    auto tone = coro::system_one(
        scheduler, client, ticket,
        {{"tone", Question::choice("Customer tone?",
                                   {{"calm", std::nullopt}, {"frustrated", std::nullopt}, {"angry", std::nullopt}})}});
    auto urgency = coro::system_one(scheduler, client, ticket,
                                    {{"urgency", Question::score("How urgent?", {"can wait", "this week", "today"})}});

    Answer billing_result = co_await billing;
    Answer tone_result = co_await tone;
    Answer urgency_result = co_await urgency;

    if (!billing_result || !tone_result || !urgency_result)
    {
        std::cerr << "one or more calls failed\n";
        co_return 1;
    }

    std::cout << "billing=" << billing_result->noul("billing")->noul << '\n';
    std::cout << "tone=" << tone_result->choice("tone")->choice << '\n';
    std::cout << "urgency=" << urgency_result->score("urgency")->score << '\n';
    co_return 0;
}

} // namespace

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::mixed_primitives());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << " client=async(coroutines)\n";

    coro::ThreadPoolExecutor scheduler{4};

    auto started = std::chrono::steady_clock::now();
    int code = coro::sync_wait(run(scheduler, rt->client));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    std::cout << "elapsed_ms=" << elapsed.count() << '\n';
    return code;
}
