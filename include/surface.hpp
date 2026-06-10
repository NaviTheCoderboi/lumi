#ifndef SURFACE_HPP
#define SURFACE_HPP

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>

#include <cstdint>

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"

#define namespace ns
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#undef namespace

#pragma clang diagnostic pop
}

struct PopupSurface {
    wl_surface* surface{nullptr};
    xdg_surface* xdgSurface{nullptr};
    xdg_popup* xdgPopup{nullptr};

    wl_egl_window* eglWindow{nullptr};
    EGLSurface eglSurface{EGL_NO_SURFACE};

    int width{0};
    int height{0};
    int sourceAppIndex{-1};
    bool isConfigured{false};
};

class LayerSurface {
   public:
    wl_surface* surface{nullptr};
    zwlr_layer_surface_v1* layerSurface{nullptr};
    wl_egl_window* eglWindow{nullptr};

    int width{0};
    int height{0};
    int pendingWidth{0};
    int pendingHeight{0};
    bool closed{false};
    bool isResizing{false};
    bool hasPendingResize{false};

    explicit LayerSurface();
    ~LayerSurface();

    LayerSurface(const LayerSurface&) = delete;
    LayerSurface& operator=(const LayerSurface&) = delete;

    void setInputRegion(int x, int y, int width, int height);
    void applyPendingResize();

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

extern PopupSurface popupSurface;
void createPopup(LayerSurface& ls, int appIndex, int iconX, int iconY, int iconWidth, int iconHeight, int menuWidth, int menuHeight, uint32_t serial);
void destroyPopup();

#endif  // SURFACE_HPP
