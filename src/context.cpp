#include "context.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <vector>

#include "config.hpp"
#include "logger.hpp"
#include "toplevel.hpp"

constexpr std::uint32_t COMPOSITOR_VERSION{4};
constexpr std::uint32_t SEAT_VERSION{7};
constexpr std::uint32_t LAYER_SHELL_VERSION{1};
constexpr std::uint32_t EXT_FOREIGN_VERSION{1};
constexpr std::uint32_t WLR_FOREIGN_VERSION{3};
constexpr std::uint32_t XDG_WM_BASE_VERSION{3};

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
        logger::error(
            "Missing wl_compositor, zwlr_layer_shell_v1, or xdg_wm_base");
        std::exit(EXIT_FAILURE);
    }

    logger::info("Wayland globals ready");
}

WaylandContext::~WaylandContext() {
    if (dataDevice) wl_data_device_destroy(dataDevice);
    if (dataDeviceManager) wl_data_device_manager_destroy(dataDeviceManager);
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

static constexpr xdg_wm_base_listener xdgWmBaseListener{
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
            wl_registry_bind(registry, name, &xdg_wm_base_interface,
                             std::min(version, XDG_WM_BASE_VERSION)));
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
    } else if (std::strcmp(interface, wl_data_device_manager_interface.name) ==
               0) {
        self.dataDeviceManager =
            static_cast<wl_data_device_manager*>(wl_registry_bind(
                registry, name, &wl_data_device_manager_interface, 3));
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
    } else if (self.dataDeviceManager &&
               wl_proxy_get_id((wl_proxy*)self.dataDeviceManager) == name) {
        wl_data_device_manager_destroy(self.dataDeviceManager);
        self.dataDeviceManager = nullptr;
    }
}

void WaylandContext::onSeatCapabilities(void* data, wl_seat* seat,
                                        uint32_t caps) {
    auto& self{*static_cast<WaylandContext*>(data)};
    bool hasPointer{static_cast<bool>(caps & WL_SEAT_CAPABILITY_POINTER)};

    if (hasPointer && !self.pointer) {
        self.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self.pointer, &pointerListener, &self);

        if (self.dataDeviceManager && !self.dataDevice) {
            self.dataDevice = wl_data_device_manager_get_data_device(
                self.dataDeviceManager, seat);
            wl_data_device_add_listener(
                self.dataDevice, &WaylandContext::dataDeviceListener, &self);
        }

        logger::info("pointer capability enabled");
    } else if (!hasPointer && self.pointer) {
        wl_pointer_destroy(self.pointer);
        self.pointer = nullptr;
        if (self.dataDevice) {
            wl_data_device_destroy(self.dataDevice);
            self.dataDevice = nullptr;
        }
        logger::warning("pointer capability disabled");
    }
}

void WaylandContext::onPointerEnter([[maybe_unused]] void* data, wl_pointer*,
                                    uint32_t, wl_surface* surface,
                                    wl_fixed_t sx, wl_fixed_t sy) {
    auto& mouseContext{MouseContext::get()};

    mouseContext.x = wl_fixed_to_double(sx);
    mouseContext.y = wl_fixed_to_double(sy);
    mouseContext.inside = true;
    mouseContext.currentSurface = surface;
}

void WaylandContext::onPointerLeave([[maybe_unused]] void* data, wl_pointer*,
                                    uint32_t, wl_surface*) {
    auto& mouseContext{MouseContext::get()};
    mouseContext.inside = false;
    mouseContext.x = -9999.f;
    mouseContext.y = -9999.f;
    mouseContext.currentSurface = nullptr;
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

static std::vector<std::string> parseUriList(const std::string& uriList) {
    std::vector<std::string> paths;
    std::istringstream stream(uriList);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.find("file://") == 0) {
            std::string path = line.substr(7);
            std::string decoded;

            for (std::size_t i{0}; i < path.length(); ++i) {
                if (path[i] == '%' && i + 2 < path.length()) {
                    int hex;
                    std::istringstream(path.substr(i + 1, 2)) >> std::hex >>
                        hex;
                    decoded += static_cast<char>(hex);
                    i += 2;
                } else {
                    decoded += path[i];
                }
            }

            paths.push_back(decoded);
        }
    }

    return paths;
}

void WaylandContext::dndOffer(void*, wl_data_offer* offer,
                              const char* mimeType) {
    auto& mouseCtx{MouseContext::get()};

    if (mouseCtx.pendingOffer == offer &&
        std::strcmp(mimeType, "text/uri-list") == 0) {
        mouseCtx.hasUriList = true;
    }
}

void WaylandContext::dndDataOffer(void*, wl_data_device*,
                                  wl_data_offer* offer) {
    auto& mouseCtx{MouseContext::get()};

    if (mouseCtx.pendingOffer) {
        wl_data_offer_destroy(mouseCtx.pendingOffer);
    }

    mouseCtx.pendingOffer = offer;
    mouseCtx.hasUriList = false;
    wl_data_offer_add_listener(offer, &WaylandContext::offerListener, nullptr);
}

void WaylandContext::dndEnter(void*, wl_data_device*, uint32_t serial,
                              wl_surface* surface, wl_fixed_t x, wl_fixed_t y,
                              wl_data_offer* offer) {
    auto& mouseCtx{MouseContext::get()};

    mouseCtx.x = wl_fixed_to_double(x);
    mouseCtx.y = wl_fixed_to_double(y);
    mouseCtx.inside = true;
    mouseCtx.currentSurface = surface;
    mouseCtx.dndActive = true;
    mouseCtx.dndOffer = offer;

    if (mouseCtx.hasUriList) {
        wl_data_offer_accept(offer, serial, "text/uri-list");
        wl_data_offer_set_actions(offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                                  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    } else {
        wl_data_offer_accept(offer, serial, nullptr);
    }
}

void WaylandContext::dndLeave(void*, wl_data_device*) {
    auto& mouseCtx{MouseContext::get()};

    if (mouseCtx.dndOffer) {
        wl_data_offer_destroy(mouseCtx.dndOffer);
    }

    mouseCtx.dndActive = false;
    mouseCtx.dndOffer = nullptr;
    mouseCtx.pendingOffer = nullptr;
    mouseCtx.dndHoverIndex = -1;
    mouseCtx.inside = false;
    mouseCtx.x = -9999.f;
    mouseCtx.y = -9999.f;
    mouseCtx.currentSurface = nullptr;
}

void WaylandContext::dndMotion(void*, wl_data_device*, uint32_t, wl_fixed_t x,
                               wl_fixed_t y) {
    auto& mouseCtx{MouseContext::get()};

    mouseCtx.x = wl_fixed_to_double(x);
    mouseCtx.y = wl_fixed_to_double(y);
}

void WaylandContext::dndDrop(void*, wl_data_device*) {
    auto& mouseCtx{MouseContext::get()};

    if (mouseCtx.dndHoverIndex == -1 || !mouseCtx.dndOffer ||
        !mouseCtx.hasUriList) {
        if (mouseCtx.dndOffer) {
            wl_data_offer_destroy(mouseCtx.dndOffer);
        }
        mouseCtx.dndActive = false;
        mouseCtx.dndOffer = nullptr;
        mouseCtx.pendingOffer = nullptr;
        mouseCtx.dndHoverIndex = -1;
        return;
    }

    int fds[2];
    if (pipe(fds) == -1) {
        wl_data_offer_destroy(mouseCtx.dndOffer);
        mouseCtx.dndActive = false;
        mouseCtx.dndOffer = nullptr;
        mouseCtx.pendingOffer = nullptr;
        mouseCtx.dndHoverIndex = -1;
        return;
    }

    wl_data_offer_receive(mouseCtx.dndOffer, "text/uri-list", fds[1]);
    close(fds[1]);
    wl_display_flush(WaylandContext::get().display);

    int flags{fcntl(fds[0], F_GETFL, 0)};
    fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);

    std::string uriList;
    char buffer[4096];

    struct pollfd pfd;
    pfd.fd = fds[0];
    pfd.events = POLLIN;

    while (true) {
        int ret = poll(&pfd, 1, 500);
        if (ret <= 0) break;

        ssize_t n = read(fds[0], buffer, sizeof(buffer) - 1);
        if (n <= 0) break;
        buffer[n] = '\0';
        uriList += buffer;
    }
    close(fds[0]);

    std::vector<std::string> paths{parseUriList(uriList)};

    if (!paths.empty()) {
        auto& config{DockConfig::get()};
        auto& item{config.items[mouseCtx.dndHoverIndex]};

        if (std::holds_alternative<DockItem>(item)) {
            std::get<DockItem>(item).app.launch(paths);
        }
    }

    wl_data_offer_finish(mouseCtx.dndOffer);
    wl_data_offer_destroy(mouseCtx.dndOffer);

    mouseCtx.dndActive = false;
    mouseCtx.dndOffer = nullptr;
    mouseCtx.pendingOffer = nullptr;
    mouseCtx.dndHoverIndex = -1;
}
