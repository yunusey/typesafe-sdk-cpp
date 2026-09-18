#pragma once

#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "mock_server.hpp"
#include "typesafe/typesafe.hpp"

namespace typesafe::examples::harness
{

inline bool is_live()
{
    const char *raw = std::getenv("TYPESAFE_LIVE");
    if (raw == nullptr)
    {
        return false;
    }
    std::string value;
    for (const char *p = raw; *p != '\0'; ++p)
    {
        value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    return value == "1" || value == "true" || value == "yes";
}

inline const char *mode_label()
{
    return is_live() ? "live" : "mock";
}

struct ExampleRuntime
{
    Client client;
    std::unique_ptr<typesafe::testing::MockServer> mock; // must outlive client in mock mode

    static Result<ExampleRuntime> start(Json mock_body)
    {
        if (is_live())
        {
            auto client = Client::from_env();
            if (!client)
            {
                return std::unexpected(client.error());
            }
            return ExampleRuntime{std::move(client.value()), nullptr};
        }

        auto server = typesafe::testing::MockServer::start();
        server->given("POST", "/v1/systemone")
            .and_header("authorization", "Bearer mock-key")
            .respond_with(typesafe::testing::ResponseTemplate(200).set_body_json(mock_body))
            .mount();

        auto client =
            Client::builder().api_key("mock-key").base_url(server->uri()).retry(RetryPolicy::disabled()).build();
        if (!client)
        {
            return std::unexpected(client.error());
        }
        return ExampleRuntime{std::move(client.value()), std::move(server)};
    }
};

} // namespace typesafe::examples::harness
