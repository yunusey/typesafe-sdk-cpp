#include "typesafe/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "typesafe/constants.hpp"

namespace typesafe::detail
{

namespace
{

std::string trim(std::string_view raw)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    auto begin = std::find_if(raw.begin(), raw.end(), not_space);
    auto end = std::find_if(raw.rbegin(), raw.rend(), not_space).base();
    if (begin >= end)
    {
        return {};
    }
    return std::string(begin, end);
}

} // namespace

std::optional<std::string> resolve_env(std::optional<std::string> value, std::string_view env,
                                       std::optional<std::string> fallback)
{
    if (value)
    {
        return value;
    }
    const char *raw = std::getenv(std::string(env).c_str());
    if (raw == nullptr)
    {
        return fallback;
    }
    std::string trimmed = trim(raw);
    if (trimmed.empty())
    {
        return fallback;
    }
    return trimmed;
}

Result<Duration> resolve_timeout(Duration timeout)
{
    if (timeout.count() <= 0.0)
    {
        return std::unexpected(Error::sdk("timeout must be a positive, finite number of seconds."));
    }
    return timeout;
}

Result<Config> Config::resolve(std::string api_key, std::optional<std::string> base_url,
                               std::optional<std::string> default_model, std::optional<Duration> timeout,
                               Headers default_headers)
{
    std::string resolved_base_url = resolve_env(std::move(base_url), kBaseUrlEnv, std::string(kDefaultBaseUrl)).value();
    while (!resolved_base_url.empty() && resolved_base_url.back() == '/')
    {
        resolved_base_url.pop_back();
    }
    std::string resolved_model =
        resolve_env(std::move(default_model), kDefaultModelEnv, std::string(kDefaultModel)).value();

    Duration requested = timeout.value_or(Duration{kDefaultTimeoutSecs});
    auto resolved_timeout = resolve_timeout(requested);
    if (!resolved_timeout)
    {
        return std::unexpected(resolved_timeout.error());
    }

    return Config{
        std::move(api_key),       std::move(resolved_base_url), std::move(resolved_model),
        resolved_timeout.value(), std::move(default_headers),
    };
}

} // namespace typesafe::detail
