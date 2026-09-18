#include <iostream>
#include <string>

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
    std::cout << "mode=" << harness::mode_label() << '\n';

    const char *ticket = "The export button spins forever after the last deploy. I need this fixed today.";
    auto response = rt->client.system_one(
        ticket,
        {
            {"category", Question::choice("Which category fits this ticket?", {{"bug_report", "Software defect"},
                                                                               {"billing", "Payment issue"},
                                                                               {"feature_request", "New capability"}})},
            {"bug_severity", Question::score("If this is a bug, how severe?", {"cosmetic", "annoying", "blocking"})},
            {"has_reproducible_steps", Question::noul("Does the ticket describe reproducible steps?")},
            {"refund_requested", Question::noul("Is the customer asking for a refund?")},
            {"frustration",
             Question::score("How frustrated does the customer sound?", {"calm", "frustrated", "angry"})},
        });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    auto category = response->choice("category").value();
    auto bug_severity = response->score("bug_severity").value();
    auto bug_repro = response->noul("has_reproducible_steps").value();
    auto refund = response->noul("refund_requested").value();
    auto frustration = response->score("frustration").value();

    std::string action = "log_only";
    if (category.choice == "bug_report")
    {
        action = (bug_severity.score > 1.5 && bug_repro.noul > 0.6) ? "escalate_engineering" : "bug_backlog";
    }
    else if (category.choice == "billing")
    {
        action = refund.noul > 0.7 ? "billing_refund_flag" : "billing_queue";
    }
    else if (category.choice == "feature_request")
    {
        action = "feature_log";
    }

    if (frustration.score > 1.5)
    {
        std::cout << "priority_flag=true\n";
    }
    std::cout << "category=" << category.choice << " action=" << action << '\n';
    return 0;
}
