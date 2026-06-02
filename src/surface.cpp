#include "surface.hpp"

#include <algorithm>
#include <cstdio>

#include "config.hpp"
#include "context.hpp"
#include "logger.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

LayerSurface::LayerSurface() {
    auto& wl{WaylandContext::get()};

    surface = wl_compositor_create_surface(wl.compositor);
    if (!surface) {
        logger::error("failed to create wl_surface");
        return;
    }

    layerSurface = zwlr_layer_shell_v1_get_layer_surface(
        wl.layerShell, surface, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "lumi");
    if (!layerSurface) {
        logger::error("failed to create layer surface");
        return;
    }

    zwlr_layer_surface_v1_add_listener(layerSurface, &layerSurfaceListener,
                                       this);

    DockConfig& config{DockConfig::get()};
    height = static_cast<int>(config.surfaceHeight());

    zwlr_layer_surface_v1_set_size(
        layerSurface, 0,
        height + static_cast<int>(config.extraAnimationSpace()));
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface, height);
    zwlr_layer_surface_v1_set_anchor(layerSurface,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    int marginTop{static_cast<int>(config.margin.top)};
    int marginRight{static_cast<int>(config.margin.right)};
    int marginBottom{static_cast<int>(config.margin.bottom)};
    int marginLeft{static_cast<int>(config.margin.left)};
    zwlr_layer_surface_v1_set_margin(layerSurface, marginTop, marginRight,
                                     marginBottom, marginLeft);

    logger::info("layer surface created");
    wl_surface_commit(surface);
}

void LayerSurface::setInputRegion(int x, int y, int width, int height) {
    auto& wl{WaylandContext::get()};

    wl_region* region{wl_compositor_create_region(wl.compositor)};

    wl_region_add(region, x, y, width, height);
    wl_surface_set_input_region(surface, region);
    wl_region_destroy(region);
}

LayerSurface::~LayerSurface() {
    if (eglWindow) wl_egl_window_destroy(eglWindow);
    if (layerSurface) zwlr_layer_surface_v1_destroy(layerSurface);
    if (surface) wl_surface_destroy(surface);
}

void LayerSurface::onConfigure(void* data, zwlr_layer_surface_v1* layerSurface,
                               std::uint32_t serial, std::uint32_t width,
                               std::uint32_t height) {
    auto& self{*static_cast<LayerSurface*>(data)};
    DockConfig& config{DockConfig::get()};

    zwlr_layer_surface_v1_ack_configure(layerSurface, serial);

    int requestedWidth{
        width ? static_cast<int>(width)
              : static_cast<int>(config.dockWidth(
                    static_cast<int>(config.items.size())))};
    int requestedHeight{
        height ? static_cast<int>(height)
               : static_cast<int>(config.surfaceHeight() +
                                  config.extraAnimationSpace())};

    requestedWidth = std::max(requestedWidth, 1);
    requestedHeight = std::max(requestedHeight, 1);

    self.width = requestedWidth;
    self.height = requestedHeight;

    char logBuffer[192];
    std::snprintf(logBuffer, sizeof(logBuffer),
                  "layer configure serial=%u width=%u height=%u final=%dx%d",
                  serial, width, height, self.width, self.height);
    logger::info(logBuffer);

    if (!self.eglWindow) {
        self.eglWindow =
            wl_egl_window_create(self.surface, self.width, self.height);
        if (!self.eglWindow) {
            logger::error("failed to create wl_egl_window");
            return;
        }
    } else {
        wl_egl_window_resize(self.eglWindow, self.width, self.height, 0, 0);
    }

    wl_surface_commit(self.surface);
}

void LayerSurface::onClosed(void* data, zwlr_layer_surface_v1*) {
    static_cast<LayerSurface*>(data)->closed = true;
}
