#include "app.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <vector>

#include "logger.hpp"
#include "utils.hpp"

static std::vector<fs::path> appDirs{([]() {
    std::vector<fs::path> dirs;

    auto home{getEnv("HOME")};
    auto xdgHome{getEnv("XDG_DATA_HOME")};
    auto xdgDirs{getEnv("XDG_DATA_DIRS")};

    if (xdgHome)
        dirs.emplace_back(fs::path(*xdgHome) / "applications");
    else if (home)
        dirs.emplace_back(fs::path(*home) / ".local/share/applications");

    std::string dataDirs{xdgDirs.value_or("/usr/local/share:/usr/share")};

    std::size_t start{0};
    while (start < dataDirs.size()) {
        auto end{dataDirs.find(":", start)};

        if (end == std::string::npos) end = dataDirs.size();

        dirs.emplace_back(fs::path(dataDirs.substr(start, end - start)));

        start = end + 1;
    }

    dirs.emplace_back("/usr/local/share/applications");
    dirs.emplace_back("/usr/share/applications");

    return dirs;
})()};

App::App(std::string className, bool isVirtual)
    : className(std::move(className)) {
    if (!isVirtual) {
        normalize();
        desktopFile = findDesktopFile();

        if (desktopFile) {
            logger::info("using desktop file: " + desktopFile->string());
            parseDesktopFile(*desktopFile);
        } else {
            logger::warning("desktop file not found for app: " + className);
        }
    }
}

bool App::matchesAppId(std::string_view appId) const {
    if (toLower(className) == toLower(appId)) return true;

    if (StartupWMClass && toLower(*StartupWMClass) == toLower(appId))
        return true;

    return icontains(appId, className) || icontains(className, appId);
}

std::vector<std::string> tokenizeExec(const std::string& exec) {
    std::vector<std::string> tokens;
    std::string current;
    bool inSingle{false};
    bool inDouble{false};

    for (std::size_t i{0}; i < exec.size(); ++i) {
        char c{exec[i]};

        if (c == '\\' && !inSingle && i + 1 < exec.size()) {
            current += exec[++i];
            continue;
        }

        if (c == '\'' && !inDouble) {
            inSingle = !inSingle;
            continue;
        }
        if (c == '"' && !inSingle) {
            inDouble = !inDouble;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) && !inSingle &&
            !inDouble) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) tokens.push_back(std::move(current));

    return tokens;
}

bool isFieldCode(const std::string& token) {
    return token.size() == 2 && token[0] == '%' && std::isalpha(token[1]);
}

void App::launch() const {
    if (!Exec) {
        logger::warning("missing Exec for app: " + className);
        return;
    }

    auto tokens{tokenizeExec(*Exec)};

    std::vector<std::string> args;
    for (auto& token : tokens) {
        if (isFieldCode(token)) continue;

        if (token == "%%") {
            args.emplace_back("%");
            continue;
        }

        args.push_back(std::move(token));
    }

    if (args.empty()) {
        logger::warning("no launch args for app: " + className);
        return;
    }

    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    pid_t pid{fork()};

    if (pid == 0) {
        setsid();

        if (const auto home{getEnv("HOME")}) {
            chdir(home->data());
        }

        int nullFd{open("/dev/null", O_RDWR)};

        if (nullFd >= 0) {
            dup2(nullFd, STDIN_FILENO);
            dup2(nullFd, STDOUT_FILENO);
            dup2(nullFd, STDERR_FILENO);

            if (nullFd > STDERR_FILENO) close(nullFd);
        }

        execvp(argv[0], argv.data());

        _exit(1);
    }
}

void App::normalize() {
    auto pos{className.find(' ')};
    if (pos != std::string::npos) className = className.substr(0, pos);

    if (className.starts_with("GIMP")) className = "gimp";
}

std::optional<fs::path> App::findDesktopFile() const {
    for (const auto& dir : appDirs) {
        fs::path exact{dir / (className + ".desktop")};
        fs::path lower{dir / (toLower(className) + ".desktop")};

        if (fs::exists(exact)) return exact;
        if (fs::exists(lower)) return lower;
    }

    return fuzzySearch();
}

std::optional<fs::path> App::fuzzySearch() const {
    std::string base{className.substr(0, className.find('-'))};

    for (const auto& dir : appDirs) {
        if (!fs::exists(dir)) continue;

        for (const auto& entry : fs::directory_iterator(dir)) {
            auto name{entry.path().filename().string()};

            if (name.contains(base) && name.ends_with(".desktop")) {
                return entry.path();
            }
        }
    }

    return std::nullopt;
}

void App::parseDesktopFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        logger::warning("failed to open desktop file: " + path.string());
        return;
    }

    bool insideActionDecl{false};
    std::optional<Action> currentAction;

    std::string line;
    while (std::getline(file, line)) {
        if (line.starts_with("Icon=")) {
            Icon = line.substr(5);
        } else if (line.starts_with("Exec=")) {
            Exec = line.substr(5);
        } else if (line.starts_with("StartupWMClass=")) {
            StartupWMClass = line.substr(16);
        } else if (line.starts_with("[Desktop Action ")) {
            if (insideActionDecl) {
                if (currentAction) actions.push_back(std::move(*currentAction));
                currentAction.reset();
            }

            insideActionDecl = true;
            currentAction.emplace();
            currentAction->name = line.substr(16, line.size() - 17);
        } else if (insideActionDecl && line.starts_with("Exec=")) {
            if (currentAction) currentAction->exec = line.substr(5);
        } else if (line.starts_with('[')) {
            insideActionDecl = false;
            if (currentAction) {
                actions.push_back(std::move(*currentAction));
                currentAction.reset();
            }
        }
    }

    if (currentAction) actions.push_back(std::move(*currentAction));
};
