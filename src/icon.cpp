#include "icon.hpp"

#include <algorithm>
#include <cstdint>

#include "gtk.hpp"
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
    roots.emplace_back("/usr/share/pixmaps");
}

std::optional<fs::path> resolveWithGtk(const std::string& iconName,
                                       int size = 64) {
    static GtkAPI gtk;

    if (!gtk.valid) return std::nullopt;

    static bool initialized{false};
    if (!initialized) {
        gtk.gtkInit();
        initialized = true;
    }

    void* display{gtk.getDisplay()};
    if (!display) return std::nullopt;

    void* theme{gtk.getTheme(display)};
    if (!theme) return std::nullopt;

    constexpr int GTK_TEXT_DIR_NONE{0};
    constexpr int GTK_ICON_LOOKUP_FORCE_REGULAR{0};

    void* paintable{gtk.lookupIcon(theme, iconName.c_str(), nullptr, size, 1,
                                   GTK_TEXT_DIR_NONE,
                                   GTK_ICON_LOOKUP_FORCE_REGULAR)};
    if (!paintable) return std::nullopt;

    void* file{gtk.getFile(paintable)};
    if (!file) {
        gtk.unref(paintable);
        return std::nullopt;
    }

    char* path{gtk.getPath(file)};

    if (!path) {
        gtk.unref(file);
        gtk.unref(paintable);
        return std::nullopt;
    }

    fs::path result(path);

    gtk.freeMem(path);
    gtk.unref(file);
    gtk.unref(paintable);

    return result;
}

std::optional<fs::path> IconIndex::_find(std::string_view name) const {
    for (const auto& root : roots) {
        if (!fs::exists(root)) continue;

        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;

            if (entry.path().stem() == name) return entry.path();
        }
    }

    return std::nullopt;
};

std::optional<fs::path> IconIndex::find(App& app) const {
    if (!app.Icon) return std::nullopt;

    auto it{icons.find(*app.Icon)};
    if (it != icons.end()) return it->second;

    fs::path p(*app.Icon);

    fs::path resolvedPath;

    if (p.is_absolute() && fs::exists(p)) {
        resolvedPath = p;
    } else if (auto gtkPath{resolveWithGtk(*app.Icon)}; gtkPath) {
        resolvedPath = *gtkPath;
    } else if (auto foundPath{_find(*app.Icon)}; foundPath) {
        resolvedPath = *foundPath;
    } else {
        logger::warning("icon not found: " + *app.Icon);
        return std::nullopt;
    }

    icons.emplace(*app.Icon, resolvedPath);

    return resolvedPath;
};

void IconIndex::preload(const std::vector<App>& apps) {
    for (const auto& app : apps) find(const_cast<App&>(app));
}
