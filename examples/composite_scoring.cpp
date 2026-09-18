#include <format>
#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Question;

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::composite_scores());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    const char *draft = "Thanks for reaching out. We will look into the duplicate charge soon.";
    auto response = rt->client.system_one(
        draft, {
                   {"clarity", Question::score("How clear is the reply?", {"poor", "ok", "excellent"})},
                   {"completeness",
                    Question::score("Does it address the customer's issue?", {"missing", "partial", "complete"})},
                   {"tone", Question::score("How warm is the tone?", {"harsh", "neutral", "warm"})},
               });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    double clarity = response->score("clarity")->score;
    double completeness = response->score("completeness")->score;
    double tone = response->score("tone")->score;

    double weighted = 0.4 * clarity + 0.4 * completeness + 0.2 * tone;
    bool publish = weighted >= 1.8;

    std::cout << std::format("clarity={} completeness={} tone={} weighted={:.2f} publish={}\n", clarity, completeness,
                             tone, weighted, publish);
    return 0;
}
