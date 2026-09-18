#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Json;
using typesafe::Question;

namespace
{
constexpr double kReviewConfidence = 0.55;
} // namespace

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::citation_check());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    Json state = {
        {"source", "Section 4.2: Refunds are issued within 5 business days for duplicate charges."},
        {"claim", "Customers receive refunds the same day for duplicate charges."},
    };

    auto response =
        rt->client.system_one(state, {
                                         {"supports_claim", Question::choice("Does the source support the claim?",
                                                                             {{"yes", "Fully supports"},
                                                                              {"partially", "Partially supports"},
                                                                              {"no", "Does not support"}})},
                                     });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    auto answer = response->choice("supports_claim").value();
    bool needs_review = answer.confidence < kReviewConfidence || answer.choice != "yes";

    std::cout << "verdict=" << answer.choice << " confidence=" << answer.confidence
              << " needs_review=" << (needs_review ? "true" : "false") << '\n';
    return 0;
}
