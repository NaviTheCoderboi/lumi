#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <wayland-client.h>

#include <cstdint>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "toplevel.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"

#define namespace ns
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#undef namespace

#pragma clang diagnostic pop
}

class MouseContext {
   public:
    float x{0.f};
    float y{0.f};
    bool inside{false};
    wl_surface* currentSurface{nullptr};

    bool pressed{false};
    float clickX{0.f};
    float clickY{0.f};
    int lastClickIndex{-1};

    bool rightPressed{false};
    float rightClickX{0.f};
    float rightClickY{0.f};
    uint32_t rightClickSerial{0};

    static MouseContext& get() {
        static MouseContext instance;
        return instance;
    }

   private:
    MouseContext() = default;
    MouseContext(const MouseContext&) = delete;
    MouseContext& operator=(const MouseContext&) = delete;
    MouseContext(MouseContext&&) = default;
    MouseContext& operator=(MouseContext&&) = default;
};

class WaylandContext {
   public:
    wl_display* display{nullptr};
    wl_compositor* compositor{nullptr};
    zwlr_layer_shell_v1* layerShell{nullptr};
    xdg_wm_base* xdgWmBase{nullptr};
    ext_foreign_toplevel_list_v1* extToplevelManager{nullptr};
    zwlr_foreign_toplevel_manager_v1* wlrToplevelManager{nullptr};

    BackendType backend{BackendType::None};

    wl_seat* seat{nullptr};
    wl_pointer* pointer{nullptr};

    ~WaylandContext();

    WaylandContext(const WaylandContext&) = delete;
    WaylandContext& operator=(const WaylandContext&) = delete;

    void roundtrip() const;
    int dispatch() const;

    static WaylandContext& get();

   private:
    WaylandContext();

    wl_registry* registry{nullptr};

    static void onGlobal(void*, wl_registry*, uint32_t, const char*, uint32_t);
    static void onGlobalRemove(void*, wl_registry*, uint32_t);
    static constexpr wl_registry_listener registryListener{
        .global = onGlobal,
        .global_remove = onGlobalRemove,
    };

    static void onSeatCapabilities(void*, wl_seat*, uint32_t);
    static constexpr wl_seat_listener seatListener{
        .capabilities = onSeatCapabilities,
        .name = [](void*, wl_seat*, const char*) {},
    };

    static void onPointerEnter(void*, wl_pointer*, uint32_t, wl_surface*,
                               wl_fixed_t, wl_fixed_t);
    static void onPointerLeave(void*, wl_pointer*, uint32_t, wl_surface*);
    static void onPointerMotion(void*, wl_pointer*, uint32_t, wl_fixed_t,
                                wl_fixed_t);
    static void onPointerButton(void*, wl_pointer*, uint32_t, uint32_t,
                                uint32_t, uint32_t);

    static constexpr wl_pointer_listener pointerListener{
        .enter = onPointerEnter,
        .leave = onPointerLeave,
        .motion = onPointerMotion,
        .button = onPointerButton,
        .axis = [](void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {},
        .frame = [](void*, wl_pointer*) {},
        .axis_source = [](void*, wl_pointer*, uint32_t) {},
        .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
        .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
        .axis_value120 = [](void*, wl_pointer*, uint32_t, int32_t) {},
        .axis_relative_direction = [](void*, wl_pointer*, uint32_t,
                                      uint32_t) {},
    };
};

#endif  // CONTEXT_HPP