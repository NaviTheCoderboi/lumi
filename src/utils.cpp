#include "utils.hpp"

#include <algorithm>
#include <cstdlib>

std::optional<std::string_view> getEnv(std::string_view name) {
    auto value{std::getenv(name.data())};
    if (!value) return std::nullopt;

    return std::string_view{value};
};

bool startsWith(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() &&
           str.substr(0, prefix.size()) == prefix;
};

bool icontains(std::string_view a, std::string_view b) {
    auto lowerA{toLower(a)};
    auto lowerB{toLower(b)};

    return lowerA.contains(lowerB);
};

std::string toLower(std::string_view str) {
    std::string result(str);
    std::ranges::transform(result, result.begin(), ::tolower);
    return result;
};

std::expected<std::string, std::string> execCommand(std::string_view command) {
    std::string result;

    FILE* pipe{popen(command.data(), "r")};
    if (!pipe) {
        return std::unexpected{"failed to execute command"};
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }

    int returnCode{pclose(pipe)};
    if (returnCode != 0) {
        return std::unexpected{"command returned non-zero exit code"};
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;
}