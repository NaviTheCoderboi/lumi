#include "toplevel.hpp"

ToplevelContext& ToplevelContext::get() {
    static ToplevelContext instance;
    return instance;
}

void ToplevelContext::setBackend(std::unique_ptr<ToplevelBackend> backend) {
    this->backend = std::move(backend);

    if (!this->backend) return;

    this->backend->setEvents({
        .appOpened = &onAppOpen,
        .appClosed = &onAppClose,
    });

    this->backend->init(&appIdCount);
}

void ToplevelContext::replayOpenApps() const {
    if (!onAppOpen) return;

    for (const auto& [appId, count] : appIdCount) {
        if (count > 0) onAppOpen(appId);
    }
}

ExtForeignBackend::ExtForeignBackend(ext_foreign_toplevel_list_v1* manager)
    : manager{manager} {}

void ExtForeignBackend::init(AppIds* appIdCount) {
    this->appIdCount = appIdCount;
    ext_foreign_toplevel_list_v1_add_listener(manager, &managerListener, this);
}

void ExtForeignBackend::onManagerToplevel(
    void* data, ext_foreign_toplevel_list_v1*,
    ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};

    ext_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);
    self.handleAppIds[handle] = "";
}

void ExtForeignBackend::onHandleAppId(void* data,
                                      ext_foreign_toplevel_handle_v1* handle,
                                      const char* appId) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};
    auto& current{self.handleAppIds[handle]};

    if (!current.empty()) {
        auto it{self.appIdCount->find(current)};

        if (it != self.appIdCount->end()) {
            if (--it->second == 0) {
                self.appIdCount->erase(it);

                if (self.events.appClosed && *self.events.appClosed)
                    (*self.events.appClosed)(current);
            }
        }
    }

    current = appId;

    auto& count{self.appIdCount->operator[](current)};

    ++count;

    if (self.events.appOpened && *self.events.appOpened)
        (*self.events.appOpened)(current);
}

void ExtForeignBackend::onHandleClosed(void* data,
                                       ext_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<ExtForeignBackend*>(data)};

    if (auto it{self.handleAppIds.find(handle)};
        it != self.handleAppIds.end()) {
        const auto appId = it->second;

        if (!appId.empty()) {
            auto countIt = self.appIdCount->find(appId);

            if (--countIt->second == 0) {
                self.appIdCount->erase(countIt);

                if (self.events.appClosed && *self.events.appClosed)
                    (*self.events.appClosed)(appId);
            }
        }

        self.handleAppIds.erase(it);
    }

    ext_foreign_toplevel_handle_v1_destroy(handle);
}

WlrForeignBackend::WlrForeignBackend(zwlr_foreign_toplevel_manager_v1* manager)
    : manager{manager} {}

void WlrForeignBackend::init(AppIds* appIdCount) {
    this->appIdCount = appIdCount;

    zwlr_foreign_toplevel_manager_v1_add_listener(manager, &managerListener,
                                                  this);
}

void WlrForeignBackend::onManagerToplevel(
    void* data, zwlr_foreign_toplevel_manager_v1*,
    zwlr_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};

    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);

    self.handleAppIds[handle] = "";
}

void WlrForeignBackend::onHandleAppId(void* data,
                                      zwlr_foreign_toplevel_handle_v1* handle,
                                      const char* appId) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};
    auto& current{self.handleAppIds[handle]};

    if (!current.empty()) {
        auto it{self.appIdCount->find(current)};

        if (it != self.appIdCount->end()) {
            if (--it->second == 0) {
                self.appIdCount->erase(it);

                if (self.events.appClosed && *self.events.appClosed)
                    (*self.events.appClosed)(current);
            }
        }
    }

    current = appId;

    auto& count{(*self.appIdCount)[current]};

    ++count;

    if (self.events.appOpened && *self.events.appOpened)
        (*self.events.appOpened)(current);
}

void WlrForeignBackend::onHandleClosed(
    void* data, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& self{*static_cast<WlrForeignBackend*>(data)};

    if (auto it{self.handleAppIds.find(handle)};
        it != self.handleAppIds.end()) {
        const auto appId = it->second;

        if (!appId.empty()) {
            auto countIt{self.appIdCount->find(appId)};

            if (countIt != self.appIdCount->end()) {
                if (--countIt->second == 0) {
                    self.appIdCount->erase(countIt);

                    if (self.events.appClosed && *self.events.appClosed)
                        (*self.events.appClosed)(appId);
                }
            }
        }

        self.handleAppIds.erase(it);
    }

    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}