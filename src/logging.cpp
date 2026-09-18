#include "logging.hpp"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

#include "typesafe/constants.hpp"

namespace typesafe::detail
{

namespace
{

std::atomic<LogLevel> g_level{LogLevel::Off};
std::once_flag g_setup;

std::string to_lower(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw)
    {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string trim(std::string_view raw)
{
    std::size_t begin = 0;
    std::size_t end = raw.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(raw[begin])))
    {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1])))
    {
        --end;
    }
    return std::string(raw.substr(begin, end - begin));
}

bool is_secret(std::string_view name)
{
    std::string lowered = to_lower(name);
    for (auto secret : kSecretHeaders)
    {
        if (lowered == secret)
        {
            return true;
        }
    }
    return lowered.find("token") != std::string::npos || lowered.find("secret") != std::string::npos;
}

const char *level_name(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Off:
    default:
        return "OFF";
    }
}

} // namespace

void logging_setup()
{
    std::call_once(g_setup, [] {
        const char *raw = std::getenv(std::string(kLogLevelEnv).c_str());
        if (raw == nullptr)
        {
            return;
        }
        std::string value = to_lower(trim(raw));
        if (value == "debug")
        {
            g_level = LogLevel::Debug;
        }
        else if (value == "info")
        {
            g_level = LogLevel::Info;
        }
        else if (value == "warn" || value == "warning")
        {
            g_level = LogLevel::Warn;
        }
        else if (value == "error")
        {
            g_level = LogLevel::Error;
        }
        else if (value == "off")
        {
            g_level = LogLevel::Off;
        }
    });
}

LogLevel log_level()
{
    return g_level.load();
}

void log_message(LogLevel level, const std::string &message)
{
    if (level == LogLevel::Off || static_cast<int>(level) > static_cast<int>(g_level.load()))
    {
        return;
    }
    std::clog << "[typesafe-sdk] " << level_name(level) << ' ' << message << '\n';
}

std::vector<std::pair<std::string, std::string>> redact(const Headers &headers)
{
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(headers.items().size());
    for (const auto &[name, value] : headers)
    {
        out.emplace_back(name, is_secret(name) ? "***" : value);
    }
    return out;
}

} // namespace typesafe::detail
