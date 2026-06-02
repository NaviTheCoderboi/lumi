#include "toplevel.hpp"

ToplevelContext& ToplevelContext::get() {
    static ToplevelContext instance;
    return instance;
}

void ToplevelContext::init(ext_foreign_toplevel_list_v1* manager) {
    ext_foreign_toplevel_list_v1_add_listener(manager, &managerListener, this);
}

void ToplevelContext::replayOpenApps() const {
    if (!onAppOpen) return;

    for (const auto& [appId, count] : appIdCount) {
        if (count > 0) onAppOpen(appId);
    }
}

void ToplevelContext::onManagerToplevel(
    void* data, ext_foreign_toplevel_list_v1*,
    ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ToplevelContext*>(data)};

    ext_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);
    self.handleAppIds[handle] = "";
}

void ToplevelContext::onHandleAppId(void* data,
                                    ext_foreign_toplevel_handle_v1* handle,
                                    const char* appId) {
    auto& self{*static_cast<ToplevelContext*>(data)};
    auto& current{self.handleAppIds[handle]};

    if (!current.empty()) {
        auto it{self.appIdCount.find(current)};

        if (it != self.appIdCount.end()) {
            if (--it->second == 0) {
                self.appIdCount.erase(it);

                if (self.onAppClose) self.onAppClose(current);
            }
        }
    }

    current = appId;

    auto& count{self.appIdCount[current]};

    ++count;

    if (self.onAppOpen) self.onAppOpen(current);
}

void ToplevelContext::onHandleClosed(void* data,
                                     ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ToplevelContext*>(data)};

    if (auto it{self.handleAppIds.find(handle)};
        it != self.handleAppIds.end()) {
        const auto appId = it->second;

        if (!appId.empty()) {
            auto countIt = self.appIdCount.find(appId);

            if (--countIt->second == 0) {
                self.appIdCount.erase(countIt);

                if (self.onAppClose) self.onAppClose(appId);
            }
        }

        self.handleAppIds.erase(it);
    }

    ext_foreign_toplevel_handle_v1_destroy(handle);
}
