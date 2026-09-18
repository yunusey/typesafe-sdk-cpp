#pragma once

#include <optional>
#include <string>

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe::detail
{

// Constructor values win over environment. Whitespace-only env values are ignored.
struct Config
{
    std::string api_key;
    std::string base_url;
    std::string default_model;
    Duration timeout{};
    Headers default_headers;

    static Result<Config> resolve(std::string api_key, std::optional<std::string> base_url,
                                  std::optional<std::string> default_model, std::optional<Duration> timeout,
                                  Headers default_headers);
};

std::optional<std::string> resolve_env(std::optional<std::string> value, std::string_view env,
                                       std::optional<std::string> fallback);
Result<Duration> resolve_timeout(Duration timeout);

} // namespace typesafe::detail
