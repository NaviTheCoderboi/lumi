#ifndef TOPLEVEL_HPP
#define TOPLEVEL_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "ext-foreign-toplevel-list-v1.h"

using Callback = void (*)(std::string_view);

class ToplevelContext {
   public:
    Callback onAppOpen{nullptr};
    Callback onAppClose{nullptr};

    static ToplevelContext& get();

    void init(ext_foreign_toplevel_list_v1* manager);
    void replayOpenApps() const;

   private:
    std::unordered_map<ext_foreign_toplevel_handle_v1*, std::string>
        handleAppIds;
    std::unordered_map<std::string, int> appIdCount;

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

    ToplevelContext() = default;
    ToplevelContext(const ToplevelContext&) = delete;
    ToplevelContext& operator=(const ToplevelContext&) = delete;
};

#endif  // TOPLEVEL_HPP
