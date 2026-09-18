#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Question;

namespace
{
constexpr double kUncertainLow = 0.30;
constexpr double kUncertainHigh = 0.70;
} // namespace

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::noul_uncertainty_band());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    const char *claim = "The claimant's story conflicts with the police report on file.";
    auto response =
        rt->client.system_one(claim, {{"fraud_likely", Question::noul("Is this claim likely fraudulent?")}});
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    double p = response->noul("fraud_likely")->noul;
    const char *action = p < kUncertainLow ? "auto_approve" : p > kUncertainHigh ? "auto_deny" : "human_review";

    std::cout << "fraud_likely=" << p << " action=" << action << '\n';
    return 0;
}
