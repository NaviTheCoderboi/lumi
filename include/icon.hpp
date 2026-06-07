#ifndef ICON_HPP
#define ICON_HPP

#include <nanosvg.h>
#include <nanosvgrast.h>
#include <nanovg.h>

#include <filesystem>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "app.hpp"
#include "cache.hpp"
#include "utils.hpp"

namespace fs = std::filesystem;

struct IconRenderer {
    explicit IconRenderer(NVGcontext* vg);
    ~IconRenderer();

    IconRenderer(const IconRenderer&) = delete;
    IconRenderer& operator=(const IconRenderer&) = delete;

    int load(const fs::path& path);
    void draw(const fs::path& path, float cx, float cy, float size,
              float cornerRadius = -1.f);

   private:
    NVGcontext* vg;
    NSVGrasterizer* rast;

    mutable LRUCache<std::string, int> cache;

    int rasterizeSVG(const fs::path& path, int size);
    int loadPNG(const fs::path& path);
};

enum class DirectoryType { Fixed, Scalable, Threshold };

struct IconEntry {
    std::uint32_t dirIndex{0};
    bool isSvg{false};

    int size{0};
    int scale{1};

    DirectoryType type{DirectoryType::Threshold};

    int minSize{0};
    int maxSize{0};
    int threshold{2};
};

struct Theme {
    std::string name;
    std::set<std::string> inherits;
    std::vector<fs::path> dirPool;
    std::unordered_map<std::string, std::vector<IconEntry>> icons;
};

class IconIndex {
   public:
    static IconIndex& get();

    std::optional<fs::path> find(const App& app) const;

    void preload(const std::vector<App>& apps);

    void clear();

   private:
    IconIndex();

    std::optional<fs::path> find(std::string_view iconName, int size = 48,
                                 int scale = 1) const;

    std::optional<std::string> currentTheme;
    std::vector<fs::path> roots;
    mutable std::unordered_map<std::string, Theme, TransparentHash,
                               TransparentEqual>
        themes;

    mutable std::unordered_map<std::string, fs::path, TransparentHash,
                               TransparentEqual>
        lookupCache;

    void loadTheme(std::string_view themeName);
    void loadThemeRecursive(std::string_view themeName);

    std::optional<fs::path> lookupFallback(std::string_view iconName) const;

    std::optional<fs::path> lookupTheme(std::string_view themeName,
                                        std::string_view iconName, int size,
                                        int scale) const;
};

#endif  // ICON_HPP