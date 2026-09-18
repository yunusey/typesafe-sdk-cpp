#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace typesafe
{

// ordered_json keeps request key order, matching the Rust SDK's preserve_order.
using Json = nlohmann::ordered_json;
using Duration = std::chrono::duration<double>;

class Error;

template <class T> using Result = std::expected<T, Error>;

// Case-insensitive. set() replaces (HeaderMap::insert); append() keeps duplicates.
class Headers
{
  public:
    Headers() = default;

    void set(std::string name, std::string value)
    {
        for (auto &[key, existing] : items_)
        {
            if (iequals(key, name))
            {
                existing = std::move(value);
                return;
            }
        }
        items_.emplace_back(std::move(name), std::move(value));
    }

    void append(std::string name, std::string value)
    {
        items_.emplace_back(std::move(name), std::move(value));
    }

    void remove(std::string_view name)
    {
        std::erase_if(items_, [&](const auto &item) { return iequals(item.first, name); });
    }

    [[nodiscard]] std::optional<std::string> get(std::string_view name) const
    {
        for (const auto &[key, value] : items_)
        {
            if (iequals(key, name))
            {
                return value;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return get(name).has_value();
    }

    [[nodiscard]] const std::vector<std::pair<std::string, std::string>> &items() const
    {
        return items_;
    }

    [[nodiscard]] auto begin() const
    {
        return items_.begin();
    }
    [[nodiscard]] auto end() const
    {
        return items_.end();
    }
    [[nodiscard]] bool empty() const
    {
        return items_.empty();
    }

    bool operator==(const Headers &) const = default;

    static bool iequals(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        return std::equal(a.begin(), a.end(), b.begin(),
                          [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
    }

  private:
    std::vector<std::pair<std::string, std::string>> items_;
};

} // namespace typesafe
