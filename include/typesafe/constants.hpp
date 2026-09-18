#pragma once

#include <array>
#include <string_view>

namespace typesafe
{

inline constexpr std::string_view kApiKeyEnv = "TYPESAFE_API_KEY";
inline constexpr std::string_view kBaseUrlEnv = "TYPESAFE_BASE_URL";
inline constexpr std::string_view kDefaultModelEnv = "TYPESAFE_DEFAULT_MODEL";
inline constexpr std::string_view kLogLevelEnv = "TYPESAFE_LOG_LEVEL";

inline constexpr std::string_view kDefaultBaseUrl = "https://api.typesafe.ai";
inline constexpr std::string_view kDefaultModel = "jev-latest";
inline constexpr double kDefaultTimeoutSecs = 10.0;

namespace detail
{

inline constexpr std::string_view kSystemOnePath = "/v1/systemone";
inline constexpr std::string_view kModelsPath = "/v1/models";
inline constexpr std::string_view kSdkName = "typesafe-sdk";
inline constexpr std::string_view kJsonContentType = "application/json";
inline constexpr std::size_t kMaxErrorBodyLength = 200;

inline constexpr std::string_view kAuthorizationHeader = "authorization";
inline constexpr std::string_view kAcceptHeader = "accept";
inline constexpr std::string_view kContentTypeHeader = "content-type";
inline constexpr std::string_view kUserAgentHeader = "user-agent";
inline constexpr std::string_view kSdkHeader = "x-typesafe-sdk";
inline constexpr std::string_view kRuntimeHeader = "x-typesafe-runtime";
inline constexpr std::string_view kRetryCountHeader = "x-typesafe-retry-count";
inline constexpr std::string_view kRequestIdHeader = "x-typesafe-request-id";
inline constexpr std::string_view kRetryAfterHeader = "retry-after";
inline constexpr std::string_view kRetryAfterMsHeader = "retry-after-ms";

inline constexpr std::array<std::string_view, 6> kSecretHeaders = {
    "authorization", "proxy-authorization", "x-api-key", "api-key", "cookie", "set-cookie",
};

} // namespace detail

} // namespace typesafe
