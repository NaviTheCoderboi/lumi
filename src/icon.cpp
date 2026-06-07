#include "icon.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>

#include "ini.hpp"
#include "logger.hpp"
#include "utils.hpp"

IconRenderer::IconRenderer(NVGcontext* vg)
    : vg(vg),
      rast(nsvgCreateRasterizer()),
      cache(64, [this](int img) {
          if (img != -1) nvgDeleteImage(this->vg, img);
      }) {}

IconRenderer::~IconRenderer() {
    nsvgDeleteRasterizer(rast);
}

int IconRenderer::rasterizeSVG(const fs::path& path, int size) {
    NSVGimage* svg{nsvgParseFromFile(path.c_str(), "px", 96.f)};
    if (!svg) {
        logger::error("failed to parse svg: " + path.string());
        return -1;
    }

    int renderSize{size * 2};
    float maxDim{std::max(svg->width, svg->height)};
    float scale{maxDim > 0.f ? renderSize / maxDim : 1.f};

    std::vector<std::uint8_t> pixels(renderSize * renderSize * 4, 0);
    nsvgRasterize(rast, svg, 0, 0, scale, pixels.data(), renderSize, renderSize,
                  renderSize * 4);

    nsvgDelete(svg);

    int img{nvgCreateImageRGBA(vg, renderSize, renderSize,
                               NVG_IMAGE_PREMULTIPLIED, pixels.data())};
    return img;
}

int IconRenderer::loadPNG(const fs::path& path) {
    return nvgCreateImage(vg, path.c_str(), 0);
}

int IconRenderer::load(const fs::path& path) {
    auto key{path.string()};

    if (auto cached = cache.get(key); cached) return *cached;

    int img{-1};
    if (path.extension() == ".svg") {
        img = rasterizeSVG(path, 128);
    } else if (path.extension() == ".png" || path.extension() == ".jpg") {
        img = loadPNG(path);
    } else {
        logger::warning("unsupported icon format: " + path.string());
    }

    cache.set(key, img);

    return img;
}

void IconRenderer::draw(const fs::path& path, float cx, float cy, float size,
                        float cornerRadius) {
    int img{load(path)};
    if (img == -1) return;

    float x{cx - size * 0.5f};
    float y{cy - size * 0.5f};
    float r{std::min(cornerRadius, size * 0.5f)};

    NVGpaint paint{nvgImagePattern(vg, x, y, size, size, 0.f, img, 1.f)};

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, r);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

std::optional<std::string> parseGtkSettings(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    std::string line;

    constexpr std::string_view key{"gtk-icon-theme-name"};
    while (std::getline(file, line)) {
        auto pos{line.find(key)};
        if (pos == std::string::npos) continue;

        pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string theme{line.substr(pos + 1)};

        theme.erase(theme.begin(), std::find_if(theme.begin(), theme.end(),
                                                [](unsigned char c) {
                                                    return !std::isspace(c);
                                                }));

        theme.erase(
            std::find_if(theme.rbegin(), theme.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            theme.end());

        if (!theme.empty()) {
            return theme;
        }
    }

    return std::nullopt;
}

std::optional<std::string> detectCurrentTheme() {
    const auto home{getEnv("HOME")};
    if (!home) {
        return std::nullopt;
    }

    if (auto theme{execCommand(
            "gsettings get org.gnome.desktop.interface icon-theme")};
        theme.has_value()) {
        std::string themeStr{*theme};

        if (themeStr.size() >= 2 && themeStr.front() == '\'' &&
            themeStr.back() == '\'') {
            themeStr = themeStr.substr(1, themeStr.size() - 2);
        }

        if (!themeStr.empty()) {
            return themeStr;
        }
    }

    if (auto theme{
            parseGtkSettings(fs::path(*home) / ".config/gtk-3.0/settings.ini")})
        return theme;
    if (auto theme{
            parseGtkSettings(fs::path(*home) / ".config/gtk-4.0/settings.ini")})
        return theme;
    if (auto theme{parseGtkSettings("/etc/gtk-3.0/settings.ini")}) return theme;
    if (auto theme{parseGtkSettings("/etc/gtk-4.0/settings.ini")}) return theme;

    return std::nullopt;
}

std::optional<fs::path> findThemeDirectory(const std::vector<fs::path>& roots,
                                           std::string_view theme) {
    for (auto& root : roots) {
        auto path{root / theme};

        if (fs::exists(path)) return path;
    }

    return std::nullopt;
}

void parseDirectory(const fs::path& themeDir, const std::string& subdir,
                    mINI::INIStructure& ini, Theme& theme) {
    auto fullDir{themeDir / subdir};
    if (!fs::exists(fullDir) || !fs::is_directory(fullDir)) {
        logger::warning("Directory " + fullDir.string() +
                        " does not exist or is not a directory, skipping");
        return;
    }

    IconEntry templateEntry;

    auto sectionIt{ini[subdir]};

    if (auto size{sectionIt["Size"]}; !size.empty()) {
        int value{0};
        auto [ptr, ec]{
            std::from_chars(size.data(), size.data() + size.size(), value)};
        if (ec == std::errc()) {
            templateEntry.size = value;
        } else {
            logger::warning("Invalid size value for " + subdir + " in theme " +
                            theme.name + ": " + size);
        }
    }

    if (auto scale{sectionIt["Scale"]}; !scale.empty()) {
        int value{0};
        auto [ptr, ec]{
            std::from_chars(scale.data(), scale.data() + scale.size(), value)};
        if (ec == std::errc()) {
            templateEntry.scale = value;
        } else {
            logger::warning("Invalid scale value for " + subdir + " in theme " +
                            theme.name + ": " + scale);
        }
    }

    if (auto minSize{sectionIt["MinSize"]}; !minSize.empty()) {
        int value{0};
        auto [ptr, ec]{std::from_chars(minSize.data(),
                                       minSize.data() + minSize.size(), value)};
        if (ec == std::errc()) {
            templateEntry.minSize = value;
        } else {
            logger::warning("Invalid min size value for " + subdir +
                            " in theme " + theme.name + ": " + minSize);
        }
    }

    if (auto maxSize{sectionIt["MaxSize"]}; !maxSize.empty()) {
        int value{0};
        auto [ptr, ec]{std::from_chars(maxSize.data(),
                                       maxSize.data() + maxSize.size(), value)};
        if (ec == std::errc()) {
            templateEntry.maxSize = value;
        } else {
            logger::warning("Invalid max size value for " + subdir +
                            " in theme " + theme.name + ": " + maxSize);
        }
    }

    std::uint32_t dirIndex{static_cast<std::uint32_t>(theme.dirPool.size())};
    theme.dirPool.push_back(fullDir);
    bool usedPool{false};

    for (const auto& file : fs::directory_iterator(fullDir)) {
        if (!file.is_regular_file()) continue;

        auto ext{file.path().extension()};
        if (ext != ".png" && ext != ".svg") continue;

        auto iconName{file.path().stem().string()};
        auto entry{templateEntry};

        entry.dirIndex = dirIndex;
        entry.isSvg = (ext == ".svg");

        theme.icons[iconName].push_back(std::move(entry));
        usedPool = true;
    }

    if (!usedPool) {
        theme.dirPool.pop_back();
    }
};

void parseIndexTheme(const fs::path& dir, Theme& theme) {
    auto indexPath{dir / "index.theme"};
    if (!fs::exists(indexPath)) return;

    mINI::INIFile file(indexPath.string());
    mINI::INIStructure ini;

    file.read(ini);

    auto inherits{ini["Icon Theme"]["inherits"]};
    if (!inherits.empty()) {
        auto splitted{inherits | std::views::split(',') |
                      std::ranges::to<std::set<std::string>>()};
        if (!splitted.empty()) {
            theme.inherits = std::move(splitted);
        }
    }

    std::vector<std::string> dirs;

    auto directories{ini["Icon Theme"]["Directories"]};
    auto scaledDirs{ini["Icon Theme"]["ScaledDirectories"]};

    if (!directories.empty()) {
        auto splitted{directories | std::views::split(',') |
                      std::ranges::to<std::vector<std::string>>()};
        dirs.insert(dirs.end(), splitted.begin(), splitted.end());
    }

    if (!scaledDirs.empty()) {
        auto splitted{scaledDirs | std::views::split(',') |
                      std::ranges::to<std::vector<std::string>>()};
        dirs.insert(dirs.end(), splitted.begin(), splitted.end());
    }

    for (auto& subdir : dirs) {
        parseDirectory(dir, subdir, ini, theme);
    }
}

bool directoryMatches(const IconEntry& e, int size, int scale) {
    if (e.scale != scale) return false;

    switch (e.type) {
        case DirectoryType::Fixed:
            return e.size == size;

        case DirectoryType::Scalable:
            return size >= e.minSize && size <= e.maxSize;

        case DirectoryType::Threshold:
            return size >= e.size - e.threshold && size <= e.size + e.threshold;
    }

    return false;
}

int directoryDistance(const IconEntry& entry, int iconSize, int iconScale) {
    const int wanted{iconSize * iconScale};

    switch (entry.type) {
        case DirectoryType::Fixed: {
            const int actual{entry.size * entry.scale};

            return std::abs(actual - wanted);
        }

        case DirectoryType::Scalable: {
            const int min{entry.minSize * entry.scale};
            const int max{entry.maxSize * entry.scale};

            if (wanted < min) return min - wanted;

            if (wanted > max) return wanted - max;

            return 0;
        }

        case DirectoryType::Threshold: {
            const int min{(entry.size - entry.threshold) * entry.scale};
            const int max{(entry.size + entry.threshold) * entry.scale};

            if (wanted < min) return min - wanted;

            if (wanted > max) return wanted - max;

            return 0;
        }
    }

    return std::numeric_limits<int>::max();
}

IconIndex& IconIndex::get() {
    static IconIndex instance;
    return instance;
}

IconIndex::IconIndex() {
    if (const auto home{getEnv("HOME")}; home) {
        roots.emplace_back(fs::path(*home) / ".local/share/icons");
        roots.emplace_back(fs::path(*home) / ".icons");
    }

    roots.emplace_back("/usr/share/icons");
    roots.emplace_back("/usr/local/share/icons");
    roots.emplace_back("/usr/share/pixmaps");

    currentTheme = detectCurrentTheme();

    if (currentTheme) loadThemeRecursive(*currentTheme);

    loadThemeRecursive("hicolor");
}

void IconIndex::loadThemeRecursive(std::string_view themeName) {
    if (themes.contains(themeName)) return;

    loadTheme(themeName);

    auto it{themes.find(themeName)};
    if (it == themes.end()) return;

    for (auto& parent : it->second.inherits) loadThemeRecursive(parent);
}

void IconIndex::loadTheme(std::string_view themeName) {
    auto themeDir{findThemeDirectory(roots, themeName)};

    if (!themeDir) return;

    Theme theme;
    theme.name = themeName;

    parseIndexTheme(*themeDir, theme);

    themes.emplace(themeName, std::move(theme));
}

void IconIndex::preload(const std::vector<App>& apps) {
    for (const auto& app : apps) {
        find(app);
    }
};

std::optional<fs::path> IconIndex::find(std::string_view icon, int size,
                                        int scale) const {
    auto key{std::format("{}:{}:{}", icon, size, scale)};

    if (auto it{lookupCache.find(key)}; it != lookupCache.end()) {
        return it->second;
    }

    if (currentTheme) {
        auto result{lookupTheme(*currentTheme, icon, size, scale)};

        if (result) {
            lookupCache[key] = *result;

            return result;
        }
    }

    auto result{lookupTheme("hicolor", icon, size, scale)};

    if (result) {
        lookupCache[key] = *result;

        return result;
    }

    auto fallback{lookupFallback(icon)};
    if (fallback) {
        lookupCache[key] = *fallback;

        return fallback;
    }
    return std::nullopt;
}

std::optional<fs::path> IconIndex::find(const App& app) const {
    if (!app.Icon) return std::nullopt;

    if (fs::exists(*app.Icon)) return *app.Icon;

    return find(*app.Icon, 48, 1);
}

std::optional<fs::path> IconIndex::lookupTheme(std::string_view themeName,
                                               std::string_view iconName,
                                               int size, int scale) const {
    auto themeIt{themes.find(std::string(themeName))};

    if (themeIt == themes.end()) return std::nullopt;

    auto iconIt{themeIt->second.icons.find(std::string(iconName))};

    if (iconIt != themeIt->second.icons.end()) {
        const IconEntry* best{nullptr};
        int bestDistance{INT_MAX};

        for (auto& entry : iconIt->second) {
            if (directoryMatches(entry, size, scale)) {
                return themeIt->second.dirPool[entry.dirIndex] /
                       (std::string(iconName) +
                        (entry.isSvg ? ".svg" : ".png"));
            }

            int distance{directoryDistance(entry, size, scale)};

            if (distance < bestDistance) {
                bestDistance = distance;
                best = &entry;
            }
        }

        if (best) {
            return themeIt->second.dirPool[best->dirIndex] /
                   (std::string(iconName) + (best->isSvg ? ".svg" : ".png"));
        }
    }

    for (auto& parent : themeIt->second.inherits) {
        auto result{lookupTheme(parent, iconName, size, scale)};

        if (result) return result;
    }

    return std::nullopt;
}

std::optional<fs::path> IconIndex::lookupFallback(
    std::string_view iconName) const {
    auto clonedRoots{roots};
    std::stable_partition(
        clonedRoots.begin(), clonedRoots.end(),
        [](const fs::path& path) { return path.filename() == "pixmaps"; });

    for (const auto& root : clonedRoots) {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;

            if (entry.path().stem() == iconName) {
                auto ext{entry.path().extension()};

                if (ext == ".png" || ext == ".svg") {
                    return entry.path();
                }
            }
        }
    }

    return std::nullopt;
}

void IconIndex::clear() {
    themes.clear();
    lookupCache.clear();
}