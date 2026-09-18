#include "typesafe/json.hpp"

#include <format>

namespace typesafe
{

namespace detail
{

const char *json_kind(const Json &value)
{
    switch (value.type())
    {
    case Json::value_t::null:
        return "null";
    case Json::value_t::boolean:
        return "boolean";
    case Json::value_t::number_integer:
    case Json::value_t::number_unsigned:
    case Json::value_t::number_float:
        return "number";
    case Json::value_t::string:
        return "string";
    case Json::value_t::array:
        return "array";
    case Json::value_t::object:
        return "object";
    default:
        return "unknown";
    }
}

} // namespace detail

Result<Json> to_state(Json value)
{
    if (value.is_string() || value.is_array() || value.is_object())
    {
        return value;
    }
    if (value.is_null())
    {
        return std::unexpected(Error::sdk("state must be a string, object, or array"));
    }
    return std::unexpected(
        Error::sdk(std::format("state must be a string, object, or array, not {}", detail::json_kind(value))));
}

} // namespace typesafe
