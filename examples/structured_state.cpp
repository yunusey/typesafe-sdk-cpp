#include <iostream>

#include "support/fixtures.hpp"
#include "support/harness.hpp"

namespace fixtures = typesafe::examples::fixtures;
namespace harness = typesafe::examples::harness;
using typesafe::Json;
using typesafe::Question;

int main()
{
    auto rt = harness::ExampleRuntime::start(fixtures::structured_ticket());
    if (!rt)
    {
        std::cerr << rt.error().to_string() << '\n';
        return 1;
    }
    std::cout << "mode=" << harness::mode_label() << '\n';

    Json state = Json::parse(R"({
    "ticket": {
      "subject": "Duplicate charge",
      "messages": [{"from": "customer", "text": "I was charged twice for order A-104."}]
    },
    "order": {
      "id": "A-104",
      "charges": [
        {"amount_usd": 49, "status": "captured"},
        {"amount_usd": 49, "status": "captured"}
      ]
    },
    "refund_policy": "Duplicate charges are eligible for a refund."
  })");

    auto response = rt->client.system_one(
        state, {
                   {"duplicate_charge", Question::noul("Does the order show duplicate captured charges?")},
                   {"refund_eligible", Question::noul("Does the refund policy cover this case?")},
               });
    if (!response)
    {
        std::cerr << response.error().to_string() << '\n';
        return 1;
    }

    std::cout << "duplicate_charge=" << response->noul("duplicate_charge")->noul
              << " refund_eligible=" << response->noul("refund_eligible")->noul << '\n';
    return 0;
}
