#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Question;

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::fan_out_triage());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << " client=blocking\n";

    auto response =
        rt->client.system_one("Billing double-charged my card.",
                              {{"category", Question::choice("Category?", {{"bug_report", std::nullopt},
                                                                           {"billing", std::nullopt},
                                                                           {"feature_request", std::nullopt}})}});
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    std::cout << "category=" << response->choice("category")->choice << '\n';
    return 0;
}
