#pragma once

#include <optional>
#include <string>

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe::detail
{

struct HttpResponse
{
    int status{0};
    Headers headers;
    std::string body;
};

Result<HttpResponse> http_execute(std::string_view method, const std::string &url, const Headers &headers,
                                  const std::optional<std::string> &body, Duration timeout);

} // namespace typesafe::detail
