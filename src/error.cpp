#include "typesafe/error.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <ctime>
#include <format>

#include "typesafe/constants.hpp"

namespace typesafe
{

ApiErrorKind api_error_kind_from_status(int status)
{
    switch (status)
    {
    case 400:
        return ApiErrorKind::BadRequest;
    case 401:
        return ApiErrorKind::Authentication;
    case 403:
        return ApiErrorKind::PermissionDenied;
    case 404:
        return ApiErrorKind::NotFound;
    case 422:
        return ApiErrorKind::UnprocessableEntity;
    case 429:
        return ApiErrorKind::RateLimited;
    default:
        if (status >= 500)
        {
            return ApiErrorKind::Internal;
        }
        return ApiErrorKind::Other;
    }
}

std::optional<ErrorBody> deserialize_body(std::string_view content)
{
    if (content.empty())
    {
        return std::nullopt;
    }
    Json parsed = Json::parse(content, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded())
    {
        return ErrorBody::from_text(std::string(content));
    }
    if (parsed.is_null())
    {
        return std::nullopt;
    }
    return ErrorBody::from_json(std::move(parsed));
}

std::optional<std::string> ApiError::request_id() const
{
    return headers.get(detail::kRequestIdHeader);
}

std::optional<double> ApiError::retry_after_ms() const
{
    if (!retry_after)
    {
        return std::nullopt;
    }
    return retry_after->count() * 1000.0;
}

std::string ApiError::to_string() const
{
    std::string out = message.empty() ? std::to_string(status) : std::format("{} {}", status, message);
    if (endpoint)
    {
        out = std::format("{}: {}", *endpoint, out);
    }
    if (auto id = request_id())
    {
        out += std::format(" (request_id={})", *id);
    }
    return out;
}

Error Error::sdk(std::string message)
{
    return Error(Sdk{std::move(message)});
}
Error Error::connection(std::string message)
{
    return Error(Connection{std::move(message)});
}
Error Error::timeout(Duration timeout)
{
    return Error(Timeout{timeout});
}
Error Error::api(ApiError error)
{
    return Error(std::move(error));
}

Error::Kind Error::kind() const
{
    switch (data_.index())
    {
    case 0:
        return Kind::Sdk;
    case 1:
        return Kind::Connection;
    case 2:
        return Kind::Timeout;
    default:
        return Kind::Api;
    }
}

const ApiError *Error::api() const
{
    return std::get_if<ApiError>(&data_);
}

std::optional<Duration> Error::timeout_value() const
{
    if (const auto *t = std::get_if<Timeout>(&data_))
    {
        return t->timeout;
    }
    return std::nullopt;
}

std::string Error::to_string() const
{
    if (const auto *s = std::get_if<Sdk>(&data_))
    {
        return s->message;
    }
    if (const auto *c = std::get_if<Connection>(&data_))
    {
        return std::format("Connection error: {}", c->message);
    }
    if (const auto *t = std::get_if<Timeout>(&data_))
    {
        return std::format("Request timed out (timeout={:.1}).", t->timeout.count());
    }
    return std::get<ApiError>(data_).to_string();
}

namespace
{

std::string truncate(const std::string &raw)
{
    // Match Rust chars().count(); clip at the (max+1)th code point.
    std::size_t count = 0;
    std::size_t clip_byte = raw.size();
    for (std::size_t i = 0; i < raw.size();)
    {
        if (count == detail::kMaxErrorBodyLength)
        {
            clip_byte = i;
        }
        unsigned char c = static_cast<unsigned char>(raw[i]);
        std::size_t width = 1;
        if ((c & 0xE0) == 0xC0)
        {
            width = 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            width = 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            width = 4;
        }
        i += width;
        ++count;
    }
    if (count > detail::kMaxErrorBodyLength)
    {
        return raw.substr(0, clip_byte) + "\u2026";
    }
    return raw;
}

std::string compact_json(const Json &value)
{
    return value.dump();
}

std::string error_message(const std::optional<ErrorBody> &body)
{
    if (!body)
    {
        return "status code (no body)";
    }
    if (body->kind == ErrorBody::Kind::Text)
    {
        return truncate(body->text);
    }
    if (auto detail = detail::extract_message(body->json))
    {
        if (!detail->empty())
        {
            return *detail;
        }
    }
    return truncate(compact_json(body->json));
}

} // namespace

namespace detail
{

std::optional<std::string> extract_message(const Json &body)
{
    if (body.is_string())
    {
        auto text = body.get<std::string>();
        if (text.empty())
        {
            return std::nullopt;
        }
        return text;
    }
    if (!body.is_object())
    {
        return std::nullopt;
    }
    if (auto it = body.find("error"); it != body.end())
    {
        if (it->is_string())
        {
            return it->get<std::string>();
        }
        if (it->is_object())
        {
            if (auto msg = it->find("message"); msg != it->end() && msg->is_string())
            {
                return msg->get<std::string>();
            }
        }
    }
    if (auto it = body.find("message"); it != body.end() && it->is_string())
    {
        return it->get<std::string>();
    }
    if (auto it = body.find("detail"); it != body.end())
    {
        const Json &detail = *it;
        if (detail.is_string())
        {
            return detail.get<std::string>();
        }
        if (detail.is_object())
        {
            if (auto msg = detail.find("message"); msg != detail.end() && msg->is_string())
            {
                return msg->get<std::string>();
            }
            return std::nullopt;
        }
        if (detail.is_array())
        {
            std::vector<std::string> parts;
            for (const auto &entry : detail)
            {
                if (!entry.is_object())
                {
                    continue;
                }
                auto msg_it = entry.find("msg");
                if (msg_it == entry.end() || !msg_it->is_string())
                {
                    continue;
                }
                std::string msg = msg_it->get<std::string>();
                std::string path;
                if (auto loc = entry.find("loc"); loc != entry.end() && loc->is_array())
                {
                    std::vector<std::string> segments;
                    for (const auto &item : *loc)
                    {
                        if (item.is_string())
                        {
                            auto text = item.get<std::string>();
                            if (text == "body")
                            {
                                continue;
                            }
                            segments.push_back(text);
                        }
                        else if (item.is_number())
                        {
                            segments.push_back(compact_json(item));
                        }
                    }
                    for (std::size_t i = 0; i < segments.size(); ++i)
                    {
                        if (i != 0)
                        {
                            path += '.';
                        }
                        path += segments[i];
                    }
                }
                if (path.empty())
                {
                    parts.push_back(msg);
                }
                else
                {
                    parts.push_back(std::format("{}: {}", path, msg));
                }
            }
            if (parts.empty())
            {
                return std::nullopt;
            }
            std::string joined;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                if (i != 0)
                {
                    joined += "; ";
                }
                joined += parts[i];
            }
            return joined;
        }
    }
    return std::nullopt;
}

Error api_error(int status, std::optional<ErrorBody> body, Headers headers, std::optional<std::string> endpoint)
{
    ApiError error;
    error.status = status;
    error.kind = api_error_kind_from_status(status);
    error.retry_after = parse_retry_after(headers);
    error.message = error_message(body);
    error.body = std::move(body);
    error.headers = std::move(headers);
    error.endpoint = std::move(endpoint);
    return Error::api(std::move(error));
}

Error validation_error(int status, std::optional<ErrorBody> body, Headers headers, std::optional<std::string> endpoint,
                       std::string field_path)
{
    ApiError error;
    error.status = status;
    error.kind = ApiErrorKind::ResponseValidation;
    error.body = std::move(body);
    error.headers = std::move(headers);
    error.endpoint = std::move(endpoint);
    error.message = std::format("Invalid response data at '{}'.", field_path);
    error.field_path = std::move(field_path);
    return Error::api(std::move(error));
}

namespace
{

std::optional<double> parse_numeric_millis(std::string_view raw, double multiplier, bool empty_is_zero)
{
    std::string trimmed(raw);
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());

    double value = 0.0;
    if (trimmed.empty())
    {
        if (!empty_is_zero)
        {
            return std::nullopt;
        }
        value = 0.0;
    }
    else
    {
        try
        {
            std::size_t consumed = 0;
            value = std::stod(trimmed, &consumed);
            if (consumed != trimmed.size())
            {
                return std::nullopt;
            }
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
    if (!std::isfinite(value) || value < 0.0)
    {
        return std::nullopt;
    }
    double delay = value * multiplier;
    if (!std::isfinite(delay))
    {
        return std::nullopt;
    }
    return delay;
}

std::optional<Duration> parse_http_date_delay(const std::string &raw)
{
    std::tm tm{};
    if (::strptime(raw.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) == nullptr) // RFC 1123
    {
        return std::nullopt;
    }
    std::time_t parsed = ::timegm(&tm);
    std::time_t now = std::time(nullptr);
    double delta = std::difftime(parsed, now);
    if (delta < 0.0)
    {
        return Duration{0.0};
    }
    return Duration{delta};
}

} // namespace

std::optional<Duration> parse_retry_after(const Headers &headers)
{
    if (auto raw = headers.get(kRetryAfterMsHeader))
    {
        if (auto millis = parse_numeric_millis(*raw, 1.0, false))
        {
            return Duration{*millis / 1000.0};
        }
    }
    if (auto raw = headers.get(kRetryAfterHeader))
    {
        if (auto millis = parse_numeric_millis(*raw, 1000.0, true))
        {
            return Duration{*millis / 1000.0};
        }
        if (auto delay = parse_http_date_delay(*raw))
        {
            return delay;
        }
    }
    return std::nullopt;
}

namespace
{

std::string strip_userinfo(std::string_view url)
{
    auto scheme_pos = url.find("://");
    if (scheme_pos == std::string_view::npos)
    {
        return std::string(url);
    }
    std::string_view scheme = url.substr(0, scheme_pos);
    std::string_view rest = url.substr(scheme_pos + 3);
    auto at = rest.find('@');
    if (at == std::string_view::npos)
    {
        return std::string(url);
    }
    return std::format("{}://{}", scheme, rest.substr(at + 1));
}

} // namespace

std::string format_endpoint(std::string_view method, std::string_view url)
{
    std::string_view without_fragment = url.substr(0, url.find('#'));
    std::string_view without_query = without_fragment.substr(0, without_fragment.find('?'));
    return std::format("{} {}", method, strip_userinfo(without_query));
}

} // namespace detail

} // namespace typesafe
