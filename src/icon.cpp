#include "icon.hpp"

#include <algorithm>
#include <cstdint>

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

void IconIndex::ensureSystemIconsIndexed() const {
    if (systemIconsIndexed) return;

    for (const auto& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec)) continue;

        auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        if (ec) continue;

        auto end = fs::end(it);
        while (it != end) {
            std::error_code fileEc;
            const auto& entry = *it;
            bool isReg = entry.is_regular_file(fileEc);
            if (!fileEc && isReg) {
                auto path = entry.path();
                auto ext = path.extension();
                if (ext == ".png" || ext == ".svg" || ext == ".xpm" || ext == ".jpg") {
                    auto name = path.stem().string();

                    auto sysIt = systemIcons.find(name);
                    if (sysIt == systemIcons.end()) {
                        systemIcons.emplace(name, path);
                    } else {
                        // Prefer SVG icons for high scaling quality
                        if (ext == ".svg") {
                            sysIt->second = path;
                        }
                    }
                }
            }
            it.increment(ec);
            if (ec) {
                break;
            }
        }
    }

    systemIconsIndexed = true;
}

std::optional<fs::path> IconIndex::_find(std::string_view name) const {
    ensureSystemIconsIndexed();

    auto it = systemIcons.find(std::string(name));
    if (it != systemIcons.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<fs::path> IconIndex::find(App& app) const {
    if (!app.Icon) return std::nullopt;

    auto it{icons.find(*app.Icon)};
    if (it != icons.end()) return it->second;

    fs::path p(*app.Icon);

    fs::path resolvedPath;

    if (p.is_absolute() && fs::exists(p)) {
        resolvedPath = p;
    } else if (auto foundPath{_find(*app.Icon)}; foundPath) {
        resolvedPath = *foundPath;
    } else {
        logger::warning("icon not found: " + *app.Icon);
        return std::nullopt;
    }

    icons.emplace(*app.Icon, resolvedPath);

    return resolvedPath;
}

void IconIndex::preload(const std::vector<App>& apps) {
    for (const auto& app : apps) find(const_cast<App&>(app));
}
