#pragma once

#include <optional>
#include <string>

#include "typesafe/common.hpp"
#include "typesafe/config.hpp"
#include "typesafe/error.hpp"

namespace typesafe::detail
{

struct PreparedRequest
{
    std::string method;
    std::string url;
    Headers headers;
    std::optional<std::string> body;
    Duration timeout{};
};

Result<PreparedRequest> prepare(const Config &config, std::string_view method, std::string_view path, const Json *body,
                                std::optional<Duration> timeout, const Headers &extra_headers);
Json merge_extra_body(Json body, const std::optional<Json> &extra);
void set_retry_count(Headers &headers, std::uint32_t attempts);

} // namespace typesafe::detail
