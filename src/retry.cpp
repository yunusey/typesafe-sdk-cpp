#include "typesafe/retry.hpp"

#include <algorithm>
#include <cmath>

#include "typesafe/config.hpp"

namespace typesafe
{

RetryPolicy RetryPolicy::disabled()
{
    RetryPolicy policy;
    policy.max_retries = 0;
    policy.timeout = std::nullopt;
    return policy;
}

Result<void> RetryPolicy::validate() const
{
    if (std::isnan(backoff_jitter) || backoff_jitter < 0.0 || backoff_jitter > 1.0)
    {
        return std::unexpected(Error::sdk("backoff_jitter must be between zero and one."));
    }
    if (timeout)
    {
        if (auto ok = detail::resolve_timeout(*timeout); !ok)
        {
            return std::unexpected(ok.error());
        }
    }
    return {};
}

bool RetryPolicy::retryable(const Error &error) const
{
    switch (error.kind())
    {
    case Error::Kind::Timeout:
        return api_timeout_error;
    case Error::Kind::Connection:
        return api_connection_error;
    case Error::Kind::Api:
        return http_statuses.contains(error.api()->status);
    case Error::Kind::Sdk:
    default:
        return false;
    }
}

Duration RetryPolicy::wait(std::uint32_t attempt_number, const Error &error, double jitter) const
{
    if (respect_retry_after)
    {
        if (const ApiError *api = error.api())
        {
            if (api->retry_after)
            {
                return *api->retry_after;
            }
        }
    }
    return detail::backoff(attempt_number, backoff_initial.count(), backoff_max.count(), backoff_jitter, jitter);
}

namespace detail
{

Duration backoff(std::uint32_t attempt, double initial, double maximum, double jitter, double random)
{
    if (initial == 0.0 || maximum == 0.0)
    {
        return Duration{0.0};
    }
    double exponent = static_cast<double>(attempt == 0 ? 0 : attempt - 1);
    double cap = std::log2(maximum) - std::log2(initial);
    double exponential = exponent >= cap ? maximum : initial * std::pow(2.0, exponent);
    double delay = exponential * (1.0 - random * jitter);
    delay = std::min(exponential, std::round(delay * 1000.0) / 1000.0);
    return Duration{std::max(0.0, delay)};
}

} // namespace detail

} // namespace typesafe
