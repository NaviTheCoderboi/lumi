#ifndef APP_HPP
#define APP_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Action {
    std::string name;
    std::string displayName;
    std::string exec;
};

class App {
   public:
    std::string className;

    App(std::string className, bool isVirtual = false);
    bool matchesAppId(std::string_view appId) const;
    void launch() const;
    void launchAction(const Action& action) const;

    // desktop entries
    std::optional<std::string> Icon;
    std::optional<std::string> Exec;
    std::optional<std::string> StartupWMClass;
    std::vector<Action> actions;

   private:
    std::optional<fs::path> desktopFile;

    void normalize();
    std::optional<fs::path> findDesktopFile() const;
    std::optional<fs::path> fuzzySearch() const;

    void parseDesktopFile(const fs::path& path);
    void launchExec(const std::string& exec) const;
};

#endif  // APP_HPP