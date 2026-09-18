#pragma once

#include <chrono>
#include <optional>
#include <set>

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe
{

// default_set() is 408/429/5xx. custom() is an exact set (empty = retry no status).
class RetryStatuses
{
  public:
    static RetryStatuses default_set()
    {
        return RetryStatuses{};
    }
    static RetryStatuses custom(std::set<int> statuses)
    {
        RetryStatuses out;
        out.custom_ = std::move(statuses);
        return out;
    }

    [[nodiscard]] bool contains(int status) const
    {
        if (custom_)
        {
            return custom_->contains(status);
        }
        return status == 408 || status == 429 || (status >= 500 && status <= 599);
    }

    [[nodiscard]] bool is_default() const
    {
        return !custom_.has_value();
    }

    bool operator==(const RetryStatuses &) const = default;

  private:
    std::optional<std::set<int>> custom_;
};

struct RetryPolicy
{
    using Seconds = std::chrono::duration<double>;

    std::uint32_t max_retries = 2;
    Seconds backoff_initial{0.5};
    Seconds backoff_max{5.0};
    double backoff_jitter = 0.25;
    RetryStatuses http_statuses = RetryStatuses::default_set();
    bool respect_retry_after = true;
    bool api_connection_error = true;
    bool api_timeout_error = true;
    std::optional<Seconds> timeout = Seconds{30.0};

    static RetryPolicy disabled();
    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] bool retryable(const Error &error) const;
    [[nodiscard]] Duration wait(std::uint32_t attempt_number, const Error &error, double jitter) const;

    bool operator==(const RetryPolicy &) const = default;
};

namespace detail
{

Duration backoff(std::uint32_t attempt, double initial, double maximum, double jitter, double random); // 1-based

} // namespace detail

} // namespace typesafe
