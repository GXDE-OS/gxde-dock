/*
 * Copyright (C) 2026 GXDE OS Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ukuiwindowmanagement.h"

#include "protocols/ukui-window-management-client-protocol.h"
#include "protocols/kywc-toplevel-v1-client-protocol.h"

#include <QGuiApplication>
#include <QDebug>
#include <QRegularExpression>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

// per-toplevel 用户数据（用于把 kywc_toplevel 事件回关联到 uuid）
struct ToplevelUserData
{
    UkuiWindowManagement *self;
    QString uuid;
};

// kywc_toplevel 事件回调（只关心 app_id 与 closed，其余为占位）
static void toplevelHandleClosed(void *data, struct kywc_toplevel_v1 *)
{
    auto *ud = static_cast<ToplevelUserData *>(data);
    ud->self->removeToplevel(ud->uuid);
    delete ud;
}

static void toplevelHandleDone(void *, struct kywc_toplevel_v1 *) {}
static void toplevelHandleTitle(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleAppId(void *data, struct kywc_toplevel_v1 *,
                                const char *appId)
{
    auto *ud = static_cast<ToplevelUserData *>(data);
    ud->self->setToplevelAppId(ud->uuid, QString::fromUtf8(appId));
}
static void toplevelHandlePrimaryOutput(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleWorkspaceEnter(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleWorkspaceLeave(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleCapabilities(void *, struct kywc_toplevel_v1 *, uint32_t) {}
static void toplevelHandleState(void *, struct kywc_toplevel_v1 *, uint32_t) {}
static void toplevelHandleParent(void *, struct kywc_toplevel_v1 *, struct kywc_toplevel_v1 *) {}
static void toplevelHandleIcon(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleGeometry(void *, struct kywc_toplevel_v1 *, int32_t, int32_t, uint32_t, uint32_t) {}
static void toplevelHandlePid(void *, struct kywc_toplevel_v1 *, uint32_t) {}

static const struct kywc_toplevel_v1_listener toplevelListener = {
    toplevelHandleClosed,
    toplevelHandleDone,
    toplevelHandleTitle,
    toplevelHandleAppId,
    toplevelHandlePrimaryOutput,
    toplevelHandleWorkspaceEnter,
    toplevelHandleWorkspaceLeave,
    toplevelHandleCapabilities,
    toplevelHandleState,
    toplevelHandleParent,
    toplevelHandleIcon,
    toplevelHandleGeometry,
    toplevelHandlePid,
};

// ---------------------------------------------------------------------------
// kywc_toplevel_manager 事件回调
// ---------------------------------------------------------------------------
static void toplevelManagerHandleToplevel(void *data, struct kywc_toplevel_manager_v1 *,
                                          struct kywc_toplevel_v1 *toplevel, const char *uuid)
{
    auto *self = static_cast<UkuiWindowManagement *>(data);
    self->addToplevel(QString::fromUtf8(uuid));

    auto *ud = new ToplevelUserData;
    ud->self = self;
    ud->uuid = QString::fromUtf8(uuid);
    kywc_toplevel_v1_add_listener(toplevel, &toplevelListener, ud);
}

static void toplevelManagerHandleFinished(void *data, struct kywc_toplevel_manager_v1 *manager)
{
    auto *self = static_cast<UkuiWindowManagement *>(data);
    Q_UNUSED(self);
    kywc_toplevel_manager_v1_destroy(manager);
}

static const struct kywc_toplevel_manager_v1_listener toplevelManagerListener = {
    toplevelManagerHandleToplevel,
    toplevelManagerHandleFinished,
};

// registry 回调
static void registryHandleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                                 const char *interface, uint32_t version)
{
    auto *self = static_cast<UkuiWindowManagement *>(data);
    if (strcmp(interface, ukui_window_management_interface.name) == 0) {
        self->bindManagement(registry, name, version);
    } else if (strcmp(interface, kywc_toplevel_manager_v1_interface.name) == 0) {
        self->bindToplevelManager(registry, name, version);
    }
}

static void registryHandleGlobalRemove(void *data, struct wl_registry *, uint32_t)
{
    Q_UNUSED(data);
}

static const struct wl_registry_listener registryListener = {
    registryHandleGlobal,
    registryHandleGlobalRemove,
};

UkuiWindowManagement *UkuiWindowManagement::instance()
{
    static auto *client = new UkuiWindowManagement;
    return client;
}

UkuiWindowManagement::UkuiWindowManagement(QObject *parent)
    : QObject(parent)
{
}

UkuiWindowManagement::~UkuiWindowManagement()
{
    // ukui_window 是合成器分配的 Wayland 代理，必须用协议销毁，不能 delete
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            ukui_window_destroy(it.value());
        }
    }
    m_windows.clear();
    if (m_toplevelManager) {
        kywc_toplevel_manager_v1_destroy(m_toplevelManager);
    }
    if (m_management) {
        ukui_window_management_destroy(m_management);
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
}

void UkuiWindowManagement::ensureInit()
{
    if (m_inited) {
        return;
    }
    m_inited = true;

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    m_display = static_cast<wl_display *>(native->nativeResourceForWindow("display", nullptr));
    if (!m_display) {
        qWarning() << "(UkuiWindowManagement) no wayland display";
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    if (m_management) {
        qInfo() << "(UkuiWindowManagement) ukui_window_management available,"
                << "minimize animation will target the dock icon";
    }
}

void UkuiWindowManagement::bindManagement(wl_registry *registry, uint32_t name, uint32_t version)
{
    m_management = static_cast<ukui_window_management *>(
        wl_registry_bind(registry, name, &ukui_window_management_interface, qMin(version, 1u)));
}

void UkuiWindowManagement::bindToplevelManager(wl_registry *registry, uint32_t name, uint32_t version)
{
    m_toplevelManager = static_cast<kywc_toplevel_manager_v1 *>(
        wl_registry_bind(registry, name, &kywc_toplevel_manager_v1_interface, qMin(version, 1u)));
    kywc_toplevel_manager_v1_add_listener(m_toplevelManager, &toplevelManagerListener, this);
}

void UkuiWindowManagement::addToplevel(const QString &uuid)
{
    m_toplevels.insert(uuid, Toplevel());
}

void UkuiWindowManagement::removeToplevel(const QString &uuid)
{
    m_toplevels.remove(uuid);
    ukui_window *win = m_windows.take(uuid);
    if (win) {
        ukui_window_destroy(win);
    }
}

// app_id 归一化与匹配：Dock 条目 Id 常为 desktop 文件名(可能带路径/".desktop")，
// 而合成器 toplevel app_id 是客户端设置值，两者需做模糊匹配才能正确关联。
static QString normalizeAppId(const QString &s)
{
    QString r = s.toLower().trimmed();
    if (r.endsWith(QLatin1String(".desktop"))) {
        r.chop(8);
    }
    int slash = r.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0) {
        r = r.mid(slash + 1);
    }
    int colon = r.lastIndexOf(QLatin1Char(':'));
    if (colon >= 0) {
        r = r.mid(colon + 1);
    }
    return r;
}

static bool appIdMatches(const QString &a, const QString &b)
{
    const QString na = normalizeAppId(a);
    const QString nb = normalizeAppId(b);
    if (na == nb) {
        return true;
    }
    if (na.isEmpty() || nb.isEmpty()) {
        return false;
    }
    // 包含匹配：较长者完整包含较短者（如 firefox 和 firefox-spark、
    // chrome 和 google-chrome）。不使用分词匹配，避免 "gxde" 这类共享
    // 前缀造成 gxde-terminal 误匹配到 gxde-multitaskingview 等问题。
    if (na.size() >= 3 && nb.contains(na)) {
        return true;
    }
    if (nb.size() >= 3 && na.contains(nb)) {
        return true;
    }
    return false;
}

void UkuiWindowManagement::setToplevelAppId(const QString &uuid, const QString &appId)
{
    auto it = m_toplevels.find(uuid);
    if (it == m_toplevels.end()) {
        return;
    }
    it->appId = appId;
    applyGeometryToToplevel(uuid, appId);
}

void UkuiWindowManagement::applyGeometryToToplevel(const QString &uuid, const QString &appId)
{
    const QString nApp = normalizeAppId(appId);
    const Geo *matched = nullptr;

    // 第一遍：精确匹配优先（不受 m_appGeometry 插入顺序影响），
    // 避免 firefox 抢先匹配到 firefox-spark 这类包含关系的窗口。
    for (auto geoIt = m_appGeometry.begin(); geoIt != m_appGeometry.end(); ++geoIt) {
        if (geoIt->panel && normalizeAppId(geoIt.key()) == nApp) {
            matched = &(*geoIt);
            break;
        }
    }
    // 第二遍：回退到包含匹配（firefox 和 firefox-spark、chrome 和 google-chrome）。
    if (!matched) {
        for (auto geoIt = m_appGeometry.begin(); geoIt != m_appGeometry.end(); ++geoIt) {
            if (geoIt->panel && appIdMatches(geoIt.key(), appId)) {
                matched = &(*geoIt);
                break;
            }
        }
    }
    if (!matched) {
        return;
    }

    const Geo &g = *matched;
    ukui_window *win = m_windows.value(uuid, nullptr);
    if (!win) {
        win = ukui_window_management_create_window(m_management, uuid.toUtf8().constData());
        if (!win) {
            return;
        }
        m_windows.insert(uuid, win);
    }
    ukui_window_set_minimized_geometry(win, g.panel, g.x, g.y, g.w, g.h);
}

void UkuiWindowManagement::applyGeometryToApp(const QString &appKey)
{
    Q_UNUSED(appKey);
    // applyGeometryToToplevel 内部已按 app_id 匹配缓存几何，这里对所有已知 toplevel 复用
    for (auto it = m_toplevels.begin(); it != m_toplevels.end(); ++it) {
        if (!it->appId.isEmpty()) {
            applyGeometryToToplevel(it.key(), it->appId);
        }
    }
}

void UkuiWindowManagement::setMinimizedGeometry(const QString &appId, struct wl_surface *panel,
                                               int x, int y, int w, int h)
{
    ensureInit();
    if (!m_management || !panel) {
        qWarning() << "[UWM] setMinimizedGeometry ignored: management=" << (void *)m_management
                   << "panel=" << (void *)panel;
        return;
    }

    const QString key = normalizeAppId(appId);
    Geo g;
    g.panel = panel;
    g.x = x;
    g.y = y;
    g.w = w;
    g.h = h;
    m_appGeometry.insert(key, g);

    applyGeometryToApp(key);
}

void UkuiWindowManagement::unsetMinimizedGeometry(const QString &appId)
{
    const QString key = normalizeAppId(appId);
    Geo g = m_appGeometry.take(key);
    if (!g.panel) {
        return;
    }
    for (auto it = m_toplevels.begin(); it != m_toplevels.end(); ++it) {
        if (appIdMatches(key, it->appId)) {
            ukui_window *win = m_windows.value(it.key());
            if (win) {
                ukui_window_unset_minimized_geometry(win, g.panel);
            }
        }
    }
}
