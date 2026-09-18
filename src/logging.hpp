#pragma once

#include <string>
#include <utility>
#include <vector>

#include "typesafe/common.hpp"

namespace typesafe::detail
{

enum class LogLevel
{
    Off,
    Error,
    Warn,
    Info,
    Debug
};

void logging_setup();
LogLevel log_level();
void log_message(LogLevel level, const std::string &message);
std::vector<std::pair<std::string, std::string>> redact(const Headers &headers);

} // namespace typesafe::detail
