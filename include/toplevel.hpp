#ifndef TOPLEVEL_HPP
#define TOPLEVEL_HPP

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

using Callback = void (*)(std::string_view);
using AppIds = std::unordered_map<std::string, int>;

struct ToplevelEvents {
    Callback* appOpened{nullptr};
    Callback* appClosed{nullptr};
};

class ToplevelBackend {
   public:
    virtual ~ToplevelBackend() = default;
    virtual void init(AppIds* appIdCount) = 0;

    void setEvents(ToplevelEvents events) { this->events = std::move(events); }

    ToplevelEvents events;
    AppIds* appIdCount;
};

class ExtForeignBackend : public ToplevelBackend {
   public:
    explicit ExtForeignBackend(ext_foreign_toplevel_list_v1* manager);

    void init(AppIds* appIdCount) override;

   private:
    ext_foreign_toplevel_list_v1* manager;
    std::unordered_map<ext_foreign_toplevel_handle_v1*, std::string>
        handleAppIds;

    static void onManagerToplevel(void*, ext_foreign_toplevel_list_v1*,
                                  ext_foreign_toplevel_handle_v1*);
    static void onHandleAppId(void*, ext_foreign_toplevel_handle_v1*,
                              const char*);
    static void onHandleClosed(void*, ext_foreign_toplevel_handle_v1*);

    static constexpr ext_foreign_toplevel_list_v1_listener managerListener{
        .toplevel = onManagerToplevel,
        .finished = [](void*, ext_foreign_toplevel_list_v1*) {},
    };

    static constexpr ext_foreign_toplevel_handle_v1_listener handleListener{
        .closed = onHandleClosed,
        .done = [](void*, ext_foreign_toplevel_handle_v1*) {},
        .title = [](void*, ext_foreign_toplevel_handle_v1*, const char*) {},
        .app_id = onHandleAppId,
        .identifier = [](void*, ext_foreign_toplevel_handle_v1*,
                         const char*) {},
    };
};

class WlrForeignBackend : public ToplevelBackend {
   public:
    explicit WlrForeignBackend(zwlr_foreign_toplevel_manager_v1* manager);

    void init(AppIds* appIdCount) override;

   private:
    zwlr_foreign_toplevel_manager_v1* manager;

    std::unordered_map<zwlr_foreign_toplevel_handle_v1*, std::string>
        handleAppIds;

    static void onManagerToplevel(void*, zwlr_foreign_toplevel_manager_v1*,
                                  zwlr_foreign_toplevel_handle_v1*);

    static void onHandleAppId(void*, zwlr_foreign_toplevel_handle_v1*,
                              const char*);

    static void onHandleClosed(void*, zwlr_foreign_toplevel_handle_v1*);

    static constexpr zwlr_foreign_toplevel_manager_v1_listener managerListener{
        .toplevel = onManagerToplevel,
        .finished = [](void*, zwlr_foreign_toplevel_manager_v1*) {},
    };

    static constexpr zwlr_foreign_toplevel_handle_v1_listener handleListener{
        .title = [](void*, zwlr_foreign_toplevel_handle_v1*, const char*) {},
        .app_id = onHandleAppId,
        .output_enter = [](void*, zwlr_foreign_toplevel_handle_v1*,
                           wl_output*) {},
        .output_leave = [](void*, zwlr_foreign_toplevel_handle_v1*,
                           wl_output*) {},
        .state = [](void*, zwlr_foreign_toplevel_handle_v1*, wl_array*) {},
        .done = [](void*, zwlr_foreign_toplevel_handle_v1*) {},
        .closed = onHandleClosed,
        .parent = [](void*, zwlr_foreign_toplevel_handle_v1*,
                     zwlr_foreign_toplevel_handle_v1*) {},
    };
};

enum BackendType { ExtForeign, WlrForeign, None };

class ToplevelContext {
   public:
    Callback onAppOpen;
    Callback onAppClose;

    static ToplevelContext& get();

    void setBackend(std::unique_ptr<ToplevelBackend> backend);

   private:
    AppIds appIdCount;

    std::unique_ptr<ToplevelBackend> backend;

    ToplevelContext() = default;
    ToplevelContext(const ToplevelContext&) = delete;
    ToplevelContext& operator=(const ToplevelContext&) = delete;
};

#endif  // TOPLEVEL_HPP
