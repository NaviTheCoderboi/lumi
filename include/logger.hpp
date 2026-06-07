#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <cstdio>
#include <string_view>

namespace logger {

inline void info(std::string_view msg) noexcept {
    std::fwrite("[INFO] ", 1, 7, stdout);
    std::fwrite(msg.data(), 1, msg.size(), stdout);
    std::fputc('\n', stdout);
}

inline void warning(std::string_view msg) noexcept {
    std::fwrite("[WARNING] ", 1, 10, stderr);
    std::fwrite(msg.data(), 1, msg.size(), stderr);
    std::fputc('\n', stderr);
}

inline void error(std::string_view msg) noexcept {
    std::fwrite("[ERROR] ", 1, 8, stderr);
    std::fwrite(msg.data(), 1, msg.size(), stderr);
    std::fputc('\n', stderr);
}

inline void debug(std::string_view msg) noexcept {
    std::fwrite("[DEBUG] ", 1, 8, stdout);
    std::fwrite(msg.data(), 1, msg.size(), stdout);
    std::fputc('\n', stdout);
}

}  // namespace logger

#endif  // LOGGER_HPP