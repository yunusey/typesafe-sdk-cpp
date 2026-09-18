#pragma once

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe
{

// Accepts string, object, or array. Rejects top-level null, boolean, and number.
Result<Json> to_state(Json value);

namespace detail
{

const char *json_kind(const Json &value);

} // namespace detail

} // namespace typesafe
