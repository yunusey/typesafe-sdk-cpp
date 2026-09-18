#include "request.hpp"

#include <format>

#include "typesafe/constants.hpp"
#include "typesafe/version.hpp"

namespace typesafe::detail
{

namespace
{

std::string compiler_id()
{
#if defined(__clang_version__)
    std::string raw = __clang_version__;
    // Keep just the version token (drop any trailing build metadata).
    auto space = raw.find(' ');
    std::string version = space == std::string::npos ? raw : raw.substr(0, space);
    return std::format("clang/{}", version);
#elif defined(__GNUC__)
    return std::format("gcc/{}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    return "cxx/unknown";
#endif
}

std::string os_id()
{
#if defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "unknown";
#endif
}

std::string arch_id()
{
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

std::string runtime_header()
{
    return std::format("{} ({}; {})", compiler_id(), os_id(), arch_id());
}

void merge_headers(Headers &target, const Headers &extra)
{
    for (const auto &[name, value] : extra)
    {
        target.set(name, value);
    }
}

} // namespace

Result<PreparedRequest> prepare(const Config &config, std::string_view method, std::string_view path, const Json *body,
                                std::optional<Duration> timeout, const Headers &extra_headers)
{
    Headers headers = config.default_headers;
    merge_headers(headers, extra_headers);
    headers.remove(kRetryCountHeader);

    std::string identity = std::format("{}/{}", kSdkName, kVersion);
    headers.set(std::string(kAuthorizationHeader), std::format("Bearer {}", config.api_key));
    headers.set(std::string(kAcceptHeader), std::string(kJsonContentType));
    headers.set(std::string(kUserAgentHeader), identity);
    headers.set(std::string(kSdkHeader), identity);
    headers.set(std::string(kRuntimeHeader), runtime_header());

    std::optional<std::string> encoded;
    if (body != nullptr)
    {
        encoded = body->dump();
        headers.set(std::string(kContentTypeHeader), std::string(kJsonContentType));
    }

    auto resolved_timeout = resolve_timeout(timeout.value_or(config.timeout));
    if (!resolved_timeout)
    {
        return std::unexpected(resolved_timeout.error());
    }

    return PreparedRequest{
        std::string(method),      std::format("{}{}", config.base_url, path), std::move(headers), std::move(encoded),
        resolved_timeout.value(),
    };
}

Json merge_extra_body(Json body, const std::optional<Json> &extra)
{
    if (extra && extra->is_object())
    {
        for (const auto &[key, value] : extra->items())
        {
            body[key] = value;
        }
    }
    return body;
}

void set_retry_count(Headers &headers, std::uint32_t attempts)
{
    if (attempts == 0)
    {
        headers.remove(kRetryCountHeader);
        return;
    }
    headers.set(std::string(kRetryCountHeader), std::to_string(attempts));
}

} // namespace typesafe::detail
