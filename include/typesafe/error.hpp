#pragma once

#include <optional>
#include <string>
#include <variant>

#include "typesafe/common.hpp"

namespace typesafe
{

enum class ApiErrorKind
{
    BadRequest,
    Authentication,
    PermissionDenied,
    NotFound,
    UnprocessableEntity,
    RateLimited,
    Internal,
    ResponseValidation, // 2xx body that does not match the schema
    Other,
};

ApiErrorKind api_error_kind_from_status(int status);

struct ErrorBody
{
    enum class Kind
    {
        Json,
        Text
    };
    Kind kind{Kind::Text};
    Json json;
    std::string text;

    static ErrorBody from_json(Json value)
    {
        return ErrorBody{Kind::Json, std::move(value), {}};
    }
    static ErrorBody from_text(std::string value)
    {
        return ErrorBody{Kind::Text, Json{}, std::move(value)};
    }

    bool operator==(const ErrorBody &) const = default;
};

std::optional<ErrorBody> deserialize_body(std::string_view content);

class ApiError
{
  public:
    int status{0};
    ApiErrorKind kind{ApiErrorKind::Other};
    std::optional<ErrorBody> body;
    Headers headers;
    std::optional<std::string> endpoint;
    std::optional<std::string> field_path;
    std::optional<Duration> retry_after;
    std::string message;

    [[nodiscard]] std::optional<std::string> request_id() const;
    [[nodiscard]] std::optional<double> retry_after_ms() const;
    [[nodiscard]] std::string to_string() const;

    bool operator==(const ApiError &) const = default;
};

class Error
{
  public:
    enum class Kind
    {
        Sdk,
        Connection,
        Timeout,
        Api
    };

    static Error sdk(std::string message);
    static Error connection(std::string message);
    static Error timeout(Duration timeout);
    static Error api(ApiError error);

    [[nodiscard]] Kind kind() const;
    [[nodiscard]] const ApiError *api() const;
    [[nodiscard]] std::optional<Duration> timeout_value() const;
    [[nodiscard]] std::string to_string() const;

    bool operator==(const Error &) const = default;

  private:
    struct Sdk
    {
        std::string message;
        bool operator==(const Sdk &) const = default;
    };
    struct Connection
    {
        std::string message;
        bool operator==(const Connection &) const = default;
    };
    struct Timeout
    {
        Duration timeout;
        bool operator==(const Timeout &) const = default;
    };

    using Data = std::variant<Sdk, Connection, Timeout, ApiError>;
    explicit Error(Data data) : data_(std::move(data))
    {
    }
    Data data_;
};

namespace detail
{

Error api_error(int status, std::optional<ErrorBody> body, Headers headers, std::optional<std::string> endpoint);
Error validation_error(int status, std::optional<ErrorBody> body, Headers headers, std::optional<std::string> endpoint,
                       std::string field_path);

// Precedence: error string, error.message, message, detail string/object/array.
std::optional<std::string> extract_message(const Json &body);
std::optional<Duration> parse_retry_after(const Headers &headers);
std::string format_endpoint(std::string_view method, std::string_view url);

} // namespace detail

} // namespace typesafe
