#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Question;

namespace
{
constexpr double kFloor = 0.5;
constexpr double kTransferConfirm = 0.9;
} // namespace

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::confidence_routing());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    const char *user_message = "Please approve the pending withdrawal on my account.";
    auto response = rt->client.system_one(
        user_message,
        {
            {"action", Question::choice("What is the user trying to do?", {{"check_balance", "View balance"},
                                                                           {"approve_transfer", "Approve withdrawal"},
                                                                           {"support", "General help"}})},
        });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    auto action = response->choice("action").value();
    double confidence = action.confidence;

    const char *route = nullptr;
    if (confidence < kFloor)
    {
        route = "human";
    }
    else if (action.choice == "check_balance")
    {
        route = "show_balance";
    }
    else if (action.choice == "approve_transfer")
    {
        route = confidence > kTransferConfirm ? "confirm_then_execute" : "ask_user_confirm";
    }
    else
    {
        route = "support_queue";
    }

    std::cout << "choice=" << action.choice << " confidence=" << confidence << " route=" << route << '\n';
    return 0;
}
