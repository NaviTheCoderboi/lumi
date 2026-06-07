#ifndef UTILS_HPP
#define UTILS_HPP

#include <expected>
#include <optional>
#include <string>
#include <string_view>

struct TransparentHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }

    std::size_t operator()(const std::string& s) const {
        return std::hash<std::string_view>{}(s);
    }

    std::size_t operator()(const char* s) const {
        return std::hash<std::string_view>{}(s);
    }
};

struct TransparentEqual {
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const {
        return lhs == rhs;
    }
};

std::optional<std::string_view> getEnv(std::string_view name);

bool startsWith(std::string_view str, std::string_view prefix);

std::string toLower(std::string_view str);

bool icontains(std::string_view a, std::string_view b);

std::expected<std::string, std::string> execCommand(std::string_view command);

#endif  // UTILS_HPP