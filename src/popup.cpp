#include "popup.hpp"

#include <nanovg.h>

#include <cstdio>

#include "app.hpp"
#include "config.hpp"
#include "context.hpp"
#include "gfx.hpp"
#include "surface.hpp"

static void drawPopupBg(NVGcontext* vg, float x, float y, float w, float h,
                        float r, NVGcolor bgColor) {
    NVGpaint shadow{nvgBoxGradient(vg, x, y + h * 0.5f, w, h * 0.5f, r, 24.f,
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.22f),
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - 20.f, y, w + 40.f, h + 32.f, r);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    NVGpaint glow{nvgLinearGradient(vg, x, y, x, y + h,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.12f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.30f));
    nvgStroke(vg);

    float midX{x + w * 0.5f};

    NVGpaint rimL{nvgLinearGradient(vg, x + r, y, midX, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + r, y + 1.f);
    nvgLineTo(vg, midX, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimL);
    nvgStroke(vg);

    NVGpaint rimR{nvgLinearGradient(vg, midX, y, x + w - r, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, midX, y + 1.f);
    nvgLineTo(vg, x + w - r, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimR);
    nvgStroke(vg);
}

Popup& Popup::get() {
    static Popup instance;
    return instance;
}

Popup::~Popup() {
    if (eglSurface != EGL_NO_SURFACE) {
        auto* gfx{GfxContext::get()};
        if (gfx) {
            eglMakeCurrent(gfx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            eglDestroySurface(gfx->display, eglSurface);
        }
    }
    if (eglWindow) wl_egl_window_destroy(eglWindow);
    if (xdgPopup) xdg_popup_destroy(xdgPopup);
    if (xdgSurface) xdg_surface_destroy(xdgSurface);
    if (surface) wl_surface_destroy(surface);
}

void Popup::onXdgConfigure(void*, xdg_surface* xdgSurface,
                           std::uint32_t serial) {
    auto& self{get()};
    xdg_surface_ack_configure(xdgSurface, serial);

    if (!self.eglWindow) {
        auto gfx{GfxContext::get()};

        self.eglWindow =
            wl_egl_window_create(self.surface, self.width, self.height);
        if (gfx) {
            self.eglSurface = eglCreateWindowSurface(
                gfx->display, gfx->config,
                reinterpret_cast<EGLNativeWindowType>(self.eglWindow), nullptr);
        }
    } else {
        wl_egl_window_resize(self.eglWindow, self.width, self.height, 0, 0);
    }

    self.configured = true;
}

void Popup::onPopupDone(void*, xdg_popup*) { get().destroy(); }

void Popup::onPopupConfigure(void*, xdg_popup*, std::int32_t, std::int32_t,
                             std::int32_t, std::int32_t) {}

void Popup::onPopupRepositioned(void*, xdg_popup*, std::uint32_t) {}

void Popup::destroy() {
    auto* gfx{GfxContext::get()};

    if (gfx && eglSurface != EGL_NO_SURFACE) {
        eglMakeCurrent(gfx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(gfx->display, eglSurface);
        eglSurface = EGL_NO_SURFACE;
        if (WaylandContext::get().layerShell) {
            eglMakeCurrent(gfx->display, gfx->surface, gfx->surface,
                           gfx->context);
        }
    }
    if (eglWindow) {
        wl_egl_window_destroy(eglWindow);
        eglWindow = nullptr;
    }
    if (xdgPopup) {
        xdg_popup_destroy(xdgPopup);
        xdgPopup = nullptr;
    }
    if (xdgSurface) {
        xdg_surface_destroy(xdgSurface);
        xdgSurface = nullptr;
    }
    if (surface) {
        wl_surface_destroy(surface);
        surface = nullptr;
    }
    configured = false;
    srcAppIndex = -1;
}

void Popup::create(LayerSurface& ls, int appIndex, int iconX, int iconY,
                   int iconWidth, int iconHeight, int menuWidth, int menuHeight,
                   std::uint32_t serial) {
    if (surface) destroy();

    auto& wl{WaylandContext::get()};

    srcAppIndex = appIndex;
    width = menuWidth;
    height = menuHeight;
    configured = false;

    surface = wl_compositor_create_surface(wl.compositor);

    xdgSurface = xdg_wm_base_get_xdg_surface(wl.xdgWmBase, surface);
    xdg_surface_add_listener(xdgSurface, &xdgSurfaceListener, nullptr);

    xdg_positioner* positioner{xdg_wm_base_create_positioner(wl.xdgWmBase)};
    xdg_positioner_set_size(positioner, menuWidth, menuHeight);
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth,
                                   iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(
        positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    xdgPopup = xdg_surface_get_popup(xdgSurface, nullptr, positioner);
    xdg_popup_add_listener(xdgPopup, &xdgPopupListener, nullptr);

    xdg_positioner_destroy(positioner);

    zwlr_layer_surface_v1_get_popup(ls.layerSurface, xdgPopup);
    xdg_popup_grab(xdgPopup, wl.seat, serial);

    wl_surface_commit(surface);

    anchorX = iconX;
    anchorY = iconY;
    anchorSize = iconWidth;
}

void Popup::reposition(int iconX, int iconY, int iconWidth, int iconHeight) {
    if (!xdgPopup) return;

    if (anchorX == iconX && anchorY == iconY && anchorSize == iconWidth) return;

    anchorX = iconX;
    anchorY = iconY;
    anchorSize = iconWidth;

    auto& wl{WaylandContext::get()};

    xdg_positioner* positioner{xdg_wm_base_create_positioner(wl.xdgWmBase)};
    xdg_positioner_set_size(positioner, width, height);
    xdg_positioner_set_anchor_rect(positioner, iconX, iconY, iconWidth,
                                   iconHeight);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_constraint_adjustment(
        positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    xdg_positioner_set_offset(positioner, 0, -8);

    xdg_popup_reposition(xdgPopup, positioner, 0);
    xdg_positioner_destroy(positioner);

    wl_surface_commit(surface);
}

void Popup::render(NVGcontext* vg) {
    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& items{config.items};

    if (srcAppIndex < 0 || srcAppIndex >= static_cast<int>(items.size()))
        return;

    float popupRadius{std::round(config.font.size * 0.3f + 8.f)};
    drawPopupBg(vg, 0.f, 0.f, width, height, popupRadius,
                config.backgroundColor.toNVG());

    auto& clickedItem{std::get<DockItem>(items.at(srcAppIndex))};
    const auto& actions{clickedItem.app.actions};

    static bool prevPressed{false};

    float rowHeight{std::round(config.font.size * 2.f + 10.f)};
    float padding{std::round(config.font.size * 0.6f + 8.f)};
    float itemY{padding / 2.f};

    float paddingX{std::round(config.font.size * 0.8f + 20.f)};
    float textX{paddingX / 2.f};

    float hoverMargin{std::round(config.font.size * 0.2f + 3.f)};
    float hoverRadius{std::round(config.font.size * 0.2f + 3.f)};
    float hoverPadY{std::round(config.font.size * 0.05f + 1.f)};

    for (std::size_t i{0}; i < actions.size(); i++) {
        float rowY{itemY + i * rowHeight};

        bool hovered{false};
        if (mouseCtx.inside && mouseCtx.currentSurface == surface &&
            mouseCtx.x >= 0 && mouseCtx.x <= width && mouseCtx.y >= rowY &&
            mouseCtx.y < rowY + rowHeight) {
            hovered = true;
        }

        if (hovered && mouseCtx.pressed && !prevPressed) {
            clickedItem.app.launchAction(actions[i]);
            destroy();
            break;
        }

        if (hovered) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, hoverMargin, rowY + hoverPadY,
                           width - hoverMargin * 2.f,
                           rowHeight - hoverPadY * 2.f, hoverRadius);
            nvgFillColor(vg, config.contextMenu.hoverColor.toNVG());
            nvgFill(vg);
        }

        nvgFontSize(vg, config.font.size);
        nvgFontFace(vg, config.font.name.c_str());
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, config.font.color.toNVG());
        nvgText(vg, textX, rowY + rowHeight / 2.0f,
                actions[i].displayName.c_str(), nullptr);
    }

    prevPressed = mouseCtx.pressed;
}
