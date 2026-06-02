#ifndef SURFACE_HPP
#define SURFACE_HPP

#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdint>

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"

#define namespace ns
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#undef namespace

#pragma clang diagnostic pop
}

class LayerSurface {
   public:
    wl_surface* surface{nullptr};
    zwlr_layer_surface_v1* layerSurface{nullptr};
    wl_egl_window* eglWindow{nullptr};

    int width{0};
    int height{0};
    bool closed{false};

    explicit LayerSurface();
    ~LayerSurface();

    LayerSurface(const LayerSurface&) = delete;
    LayerSurface& operator=(const LayerSurface&) = delete;

    void setInputRegion(int x, int y, int width, int height);

   private:
    static void onConfigure(void* data, zwlr_layer_surface_v1* layerSurface,
                            std::uint32_t serial, std::uint32_t width,
                            std::uint32_t height);
    static void onClosed(void* data, zwlr_layer_surface_v1* layerSurface);

    static inline const zwlr_layer_surface_v1_listener layerSurfaceListener{
        .configure = onConfigure,
        .closed = onClosed,
    };
};

#endif  // SURFACE_HPP
