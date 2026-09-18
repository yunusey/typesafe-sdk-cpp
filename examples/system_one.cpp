#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Json;
using typesafe::Question;

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::mixed_primitives());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    Json state = {{"document", "I was charged twice. Please fix this ASAP."}};
    auto response = rt->client.system_one(
        state,
        {
            {"billing", Question::noul("Is this ticket about billing?")},
            {"tone", Question::choice("What is the customer's tone?",
                                      {{"calm", std::nullopt}, {"frustrated", std::nullopt}, {"angry", std::nullopt}})},
            {"urgency", Question::score("How urgent is this ticket?", {"can wait", "this week", "today"})},
        });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    std::cout << "billing=" << response->noul("billing")->noul << '\n';
    std::cout << "tone=" << response->choice("tone")->choice << '\n';
    std::cout << "urgency=" << response->score("urgency")->score << '\n';
    return 0;
}
