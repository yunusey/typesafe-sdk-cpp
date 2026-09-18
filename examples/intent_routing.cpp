#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Question;

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::intent_routing());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    const char *request = "Summarize this quarter's revenue drivers in two paragraphs.";
    auto response = rt->client.system_one(
        request,
        {
            {"intent", Question::choice("Best handler for this request", {{"deterministic", "Fixed code path"},
                                                                          {"specialist_llm", "Large model task"},
                                                                          {"human", "Needs a person"}})},
        });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    auto intent = response->choice("intent").value();
    const char *handler = intent.choice == "deterministic"    ? "rules_engine"
                          : intent.choice == "specialist_llm" ? "frontier_model"
                                                              : "human_queue";

    std::cout << "intent=" << intent.choice << " confidence=" << intent.confidence << " handler=" << handler << '\n';
    return 0;
}
