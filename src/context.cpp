#include "context.hpp"

#include <wayland-client-protocol.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include "logger.hpp"
#include "toplevel.hpp"

constexpr std::uint32_t COMPOSITOR_VERSION{4};
constexpr std::uint32_t SEAT_VERSION{7};
constexpr std::uint32_t LAYER_SHELL_VERSION{1};
constexpr std::uint32_t EXT_FOREIGN_VERSION{1};
constexpr std::uint32_t WLR_FOREIGN_VERSION{3};

WaylandContext& WaylandContext::get() {
    static WaylandContext instance;
    return instance;
}

WaylandContext::WaylandContext() {
    display = wl_display_connect(nullptr);

    if (!display) {
        logger::error("Cannot connect to Wayland display");
        std::exit(EXIT_FAILURE);
    }

    logger::info("Wayland display connected");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, this);
    roundtrip();

    if (!compositor || !layerShell || !xdgWmBase) {
        logger::error("Missing wl_compositor, zwlr_layer_shell_v1, or xdg_wm_base");
        std::exit(EXIT_FAILURE);
    }

    logger::info("Wayland globals ready");
}

WaylandContext::~WaylandContext() {
    if (pointer) wl_pointer_destroy(pointer);
    if (seat) wl_seat_destroy(seat);
    if (xdgWmBase) xdg_wm_base_destroy(xdgWmBase);
    if (layerShell) zwlr_layer_shell_v1_destroy(layerShell);
    if (compositor) wl_compositor_destroy(compositor);
    if (registry) wl_registry_destroy(registry);
    if (display) wl_display_disconnect(display);
    if (extToplevelManager)
        ext_foreign_toplevel_list_v1_destroy(extToplevelManager);
    if (wlrToplevelManager)
        zwlr_foreign_toplevel_manager_v1_destroy(wlrToplevelManager);
}

void WaylandContext::roundtrip() const { wl_display_roundtrip(display); }

int WaylandContext::dispatch() const { return wl_display_dispatch(display); }

static void xdg_wm_base_ping(void*, xdg_wm_base* xdgWmBase, uint32_t serial) {
    xdg_wm_base_pong(xdgWmBase, serial);
}

static constexpr xdg_wm_base_listener xdgWmBaseListener = {
    .ping = xdg_wm_base_ping,
};

void WaylandContext::onGlobal(void* data, wl_registry* registry,
                              std::uint32_t name, const char* interface,
                              std::uint32_t version) {
    auto& self{*static_cast<WaylandContext*>(data)};
    auto& toplevelCtx{ToplevelContext::get()};

    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        self.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface,
                             std::min(version, COMPOSITOR_VERSION)));
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) ==
               0) {
        self.layerShell = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface,
                             std::min(version, LAYER_SHELL_VERSION)));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        self.xdgWmBase = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(self.xdgWmBase, &xdgWmBaseListener, nullptr);
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self.seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, name, &wl_seat_interface,
                             std::min(version, SEAT_VERSION)));
        wl_seat_add_listener(self.seat, &seatListener, &self);
    } else if (std::strcmp(interface,
                           ext_foreign_toplevel_list_v1_interface.name) == 0 &&
               self.backend == BackendType::None) {
        self.extToplevelManager =
            static_cast<ext_foreign_toplevel_list_v1*>(wl_registry_bind(
                registry, name, &ext_foreign_toplevel_list_v1_interface,
                std::min(version, EXT_FOREIGN_VERSION)));

        logger::info("Found ext-foreign-toplevel-list-v1");

        self.backend = BackendType::ExtForeign;
        toplevelCtx.setBackend(
            std::make_unique<ExtForeignBackend>(self.extToplevelManager));
    } else if (std::strcmp(interface,
                           zwlr_foreign_toplevel_manager_v1_interface.name) ==
                   0 &&
               self.backend == BackendType::None) {
        self.wlrToplevelManager =
            static_cast<zwlr_foreign_toplevel_manager_v1*>(wl_registry_bind(
                registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
                std::min(version, WLR_FOREIGN_VERSION)));

        logger::info("Found wlr-foreign-toplevel-management");

        self.backend = BackendType::WlrForeign;
        toplevelCtx.setBackend(
            std::make_unique<WlrForeignBackend>(self.wlrToplevelManager));
    }
}

void WaylandContext::onGlobalRemove(void* data,
                                    [[maybe_unused]] wl_registry* registry,
                                    std::uint32_t name) {
    auto& self{*static_cast<WaylandContext*>(data)};

    if (self.compositor &&
        wl_proxy_get_id((wl_proxy*)self.compositor) == name) {
        wl_compositor_destroy(self.compositor);
        self.compositor = nullptr;
    } else if (self.layerShell &&
               wl_proxy_get_id((wl_proxy*)self.layerShell) == name) {
        zwlr_layer_shell_v1_destroy(self.layerShell);
        self.layerShell = nullptr;
    } else if (self.xdgWmBase &&
               wl_proxy_get_id((wl_proxy*)self.xdgWmBase) == name) {
        xdg_wm_base_destroy(self.xdgWmBase);
        self.xdgWmBase = nullptr;
    }
}

void WaylandContext::onSeatCapabilities(void* data, wl_seat* seat,
                                        uint32_t caps) {
    auto& self{*static_cast<WaylandContext*>(data)};
    bool hasPointer{static_cast<bool>(caps & WL_SEAT_CAPABILITY_POINTER)};

    if (hasPointer && !self.pointer) {
        self.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self.pointer, &pointerListener, &self);
        logger::info("pointer capability enabled");
    } else if (!hasPointer && self.pointer) {
        wl_pointer_destroy(self.pointer);
        self.pointer = nullptr;
        logger::warning("pointer capability disabled");
    }
}

void WaylandContext::onPointerEnter([[maybe_unused]] void* data, wl_pointer*,
                                    uint32_t, wl_surface*, wl_fixed_t sx,
                                    wl_fixed_t sy) {
    auto& mouseContext{MouseContext::get()};

    mouseContext.x = wl_fixed_to_double(sx);
    mouseContext.y = wl_fixed_to_double(sy);
    mouseContext.inside = true;
}

void WaylandContext::onPointerLeave([[maybe_unused]] void* data, wl_pointer*,
                                    uint32_t, wl_surface*) {
    auto& mouseContext{MouseContext::get()};
    mouseContext.inside = false;
    mouseContext.x = -9999.f;
    mouseContext.y = -9999.f;
}

void WaylandContext::onPointerMotion([[maybe_unused]] void* data, wl_pointer*,
                                     uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    auto& mouseContext{MouseContext::get()};
    mouseContext.x = wl_fixed_to_double(sx);
    mouseContext.y = wl_fixed_to_double(sy);
}

void WaylandContext::onPointerButton([[maybe_unused]] void* data, wl_pointer*,
                                     [[maybe_unused]] uint32_t serial,
                                     uint32_t /*time*/, uint32_t button,
                                     uint32_t state) {
    if (button != 0x110 && button != 0x111) return;

    auto& mouseContext{MouseContext::get()};

    if (button == 0x110) {
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            mouseContext.pressed = true;
            mouseContext.clickX = mouseContext.x;
            mouseContext.clickY = mouseContext.y;
        } else {
            mouseContext.pressed = false;
        }
    } else if (button == 0x111) {
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            mouseContext.rightPressed = true;
            mouseContext.rightClickX = mouseContext.x;
            mouseContext.rightClickY = mouseContext.y;
            mouseContext.rightClickSerial = serial;
        } else {
            mouseContext.rightPressed = false;
        }
    }
}
