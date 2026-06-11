#include "surface.hpp"

#include <algorithm>
#include <cstdio>

#include "config.hpp"
#include "context.hpp"
#include "logger.hpp"
#include "gfx.hpp"
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
    height = static_cast<int>(config.height());

    zwlr_layer_surface_v1_set_size(layerSurface, 0, height);
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface,
                                             config.surfaceHeight());
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

void LayerSurface::applyPendingResize() {
    if (!hasPendingResize || !eglWindow) return;

    wl_egl_window_resize(eglWindow, pendingWidth, pendingHeight, 0, 0);
    hasPendingResize = false;
    isResizing = false;

    wl_surface_commit(surface);
}

LayerSurface::~LayerSurface() {
    if (eglWindow) wl_egl_window_destroy(eglWindow);
    if (layerSurface) {
        zwlr_layer_surface_v1_destroy(layerSurface);
    }
    if (surface) {
        wl_surface_destroy(surface);
    }
}

PopupSurface popupSurface;

static void xdg_surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    
    if (!popupSurface.eglWindow) {
        popupSurface.eglWindow = wl_egl_window_create(popupSurface.surface, popupSurface.width, popupSurface.height);
        if (g_gfxContext) {
            popupSurface.eglSurface = eglCreateWindowSurface(
                g_gfxContext->display, g_gfxContext->config,
                reinterpret_cast<EGLNativeWindowType>(popupSurface.eglWindow), nullptr);
        }
    } else {
        wl_egl_window_resize(popupSurface.eglWindow, popupSurface.width, popupSurface.height, 0, 0);
    }
    
    popupSurface.isConfigured = true;
}

static const xdg_surface_listener xdg_surface_listener_impl = {
    .configure = xdg_surface_configure,
};

static void xdg_popup_popup_done(void* data, xdg_popup* xdg_popup) {
    destroyPopup();
}

static void xdg_popup_configure(void* data, xdg_popup* xdg_popup, int32_t x, int32_t y, int32_t width, int32_t height) {
    // Optionally use the width/height provided by the compositor
}

static const xdg_popup_listener xdg_popup_listener_impl = {
    .configure = xdg_popup_configure,
    .popup_done = xdg_popup_popup_done,
};

void destroyPopup() {
    if (g_gfxContext && popupSurface.eglSurface != EGL_NO_SURFACE) {
        eglMakeCurrent(g_gfxContext->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(g_gfxContext->display, popupSurface.eglSurface);
        popupSurface.eglSurface = EGL_NO_SURFACE;
        // Restore context to main surface
        if (WaylandContext::get().layerShell) {
             eglMakeCurrent(g_gfxContext->display, g_gfxContext->surface, g_gfxContext->surface, g_gfxContext->context);
        }
    }
    if (popupSurface.eglWindow) {
        wl_egl_window_destroy(popupSurface.eglWindow);
        popupSurface.eglWindow = nullptr;
    }
    if (popupSurface.xdgPopup) {
        xdg_popup_destroy(popupSurface.xdgPopup);
        popupSurface.xdgPopup = nullptr;
    }
    if (popupSurface.xdgSurface) {
        xdg_surface_destroy(popupSurface.xdgSurface);
        popupSurface.xdgSurface = nullptr;
    }
    if (popupSurface.surface) {
        wl_surface_destroy(popupSurface.surface);
        popupSurface.surface = nullptr;
    }
    popupSurface.isConfigured = false;
    popupSurface.sourceAppIndex = -1;
}

void createPopup(LayerSurface& ls, int appIndex, int iconX, int iconY, int iconWidth, int iconHeight, int menuWidth, int menuHeight, uint32_t serial) {
    if (popupSurface.surface) {
        destroyPopup();
    }

    auto& wl{WaylandContext::get()};
    
    popupSurface.sourceAppIndex = appIndex;
    popupSurface.width = menuWidth;
    popupSurface.height = menuHeight;
    popupSurface.isConfigured = false;

    popupSurface.surface = wl_compositor_create_surface(wl.compositor);
    
    popupSurface.xdgSurface = xdg_wm_base_get_xdg_surface(wl.xdgWmBase, popupSurface.surface);
    xdg_surface_add_listener(popupSurface.xdgSurface, &xdg_surface_listener_impl, nullptr);

    xdg_positioner* positioner = xdg_wm_base_create_positioner(wl.xdgWmBase);
    xdg_positioner_set_size(positioner, menuWidth, menuHeight);
    
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth, iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    popupSurface.xdgPopup = xdg_surface_get_popup(popupSurface.xdgSurface, nullptr, positioner);
    xdg_popup_add_listener(popupSurface.xdgPopup, &xdg_popup_listener_impl, nullptr);
    
    xdg_positioner_destroy(positioner);

    zwlr_layer_surface_v1_get_popup(ls.layerSurface, popupSurface.xdgPopup);

    xdg_popup_grab(popupSurface.xdgPopup, wl.seat, serial);

    wl_surface_commit(popupSurface.surface);

    popupSurface.anchorX = iconX;
    popupSurface.anchorY = iconY;
    popupSurface.anchorSize = iconWidth;
}

void repositionPopup(int iconX, int iconY, int iconWidth, int iconHeight) {
    if (!popupSurface.xdgPopup) return;
    
    if (popupSurface.anchorX == iconX && popupSurface.anchorY == iconY && popupSurface.anchorSize == iconWidth) {
        return;
    }
    
    popupSurface.anchorX = iconX;
    popupSurface.anchorY = iconY;
    popupSurface.anchorSize = iconWidth;

    auto& wl{WaylandContext::get()};
    
    xdg_positioner* positioner = xdg_wm_base_create_positioner(wl.xdgWmBase);
    xdg_positioner_set_size(positioner, popupSurface.width, popupSurface.height);
    
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth, iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    xdg_popup_reposition(popupSurface.xdgPopup, positioner, 0);
    xdg_positioner_destroy(positioner);

    wl_surface_commit(popupSurface.surface);
}

void LayerSurface::onConfigure(void* data, zwlr_layer_surface_v1* layerSurface,
                               std::uint32_t serial, std::uint32_t width,
                               std::uint32_t height) {
    auto& self{*static_cast<LayerSurface*>(data)};
    DockConfig& config{DockConfig::get()};

    zwlr_layer_surface_v1_ack_configure(layerSurface, serial);
    self.isResizing = false;

    int desiredHeight = static_cast<int>(config.height());

    int requestedHeight{
        std::max(static_cast<int>(height ? height : desiredHeight), 1)};
    int requestedWidth{std::max(
        static_cast<int>(width ? width : config.dockWidth(config.items.size())),
        1)};

    bool sizeChanged = false;
    if (requestedWidth != self.width) {
        self.width = requestedWidth;
        sizeChanged = true;
    }

    if (self.height != requestedHeight) {
        self.height = requestedHeight;
        sizeChanged = true;

        zwlr_layer_surface_v1_set_size(self.layerSurface, 0, self.height);
        zwlr_layer_surface_v1_set_exclusive_zone(self.layerSurface,
                                                 config.surfaceHeight());
        zwlr_layer_surface_v1_set_anchor(
            self.layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

        int marginTop{static_cast<int>(config.margin.top)};
        int marginRight{static_cast<int>(config.margin.right)};
        int marginBottom{static_cast<int>(config.margin.bottom)};
        int marginLeft{static_cast<int>(config.margin.left)};
        zwlr_layer_surface_v1_set_margin(self.layerSurface, marginTop,
                                         marginRight, marginBottom, marginLeft);
    }

    if (sizeChanged && self.eglWindow) {
        self.pendingWidth = self.width;
        self.pendingHeight = self.height;
        self.hasPendingResize = true;
        self.isResizing = true;
    }

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
    }

    if (!self.hasPendingResize) {
        wl_surface_commit(self.surface);
    }
}

void LayerSurface::onClosed(void* data, zwlr_layer_surface_v1*) {
    static_cast<LayerSurface*>(data)->closed = true;
}
