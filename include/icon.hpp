#ifndef ICON_HPP
#define ICON_HPP

#include <nanosvg.h>
#include <nanosvgrast.h>
#include <nanovg.h>

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include "app.hpp"
#include "cache.hpp"

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

class IconIndex {
   public:
    static IconIndex& get();

    std::optional<fs::path> find(App& app) const;
    void preload(const std::vector<App>& apps);

   private:
    IconIndex();

    std::vector<fs::path> roots;

    IconIndex(const IconIndex&) = delete;
    IconIndex& operator=(const IconIndex&) = delete;
    IconIndex(IconIndex&&) = delete;
    IconIndex& operator=(IconIndex&&) = delete;

    mutable std::unordered_map<std::string, fs::path> icons;
    mutable std::unordered_map<std::string, fs::path> systemIcons;
    mutable bool systemIconsIndexed{false};
    void ensureSystemIconsIndexed() const;
    std::optional<fs::path> _find(std::string_view name) const;
};

#endif  // ICON_HPP