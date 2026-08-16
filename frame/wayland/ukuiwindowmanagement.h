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

#ifndef FRAME_WAYLAND_UKUIWINDOWMANAGEMENT_H_
#define FRAME_WAYLAND_UKUIWINDOWMANAGEMENT_H_

#include <QObject>
#include <QHash>
#include <QString>

struct wl_display;
struct wl_registry;
struct wl_surface;
struct ukui_window_management;
struct ukui_window;
struct kywc_toplevel_manager_v1;
struct kywc_toplevel_v1;

/**
 * 客户端封装：通过 ukui_window_management 协议把 Dock 任务栏图标的几何位置
 * 告知合成器，使最小化/还原动画（magic_lamp / scale）能精确收拢到对应图标，
 * 而不是回退到窗口自身中心。
 *
 * 关联方式：合成器用 kywc_view->uuid 作为 ukui_window 标识，而 Dock 已经通过
 * kywc_toplevel_v1 拿到了同一个 uuid，因此本类同时跟踪 kywc_toplevel 以建立
 * "uuid <-> app_id" 的映射，再按 app_id 把缓存的图标几何下发到每个匹配窗口。
 */
class UkuiWindowManagement : public QObject
{
    Q_OBJECT

public:
    static UkuiWindowManagement *instance();

    /**
     * 设置某个 app 的 Dock 图标几何。坐标为相对 panel surface 的局部坐标（逻辑像素）。
     * 会立即应用到所有已匹配的 toplevel，并在后续新出现的 toplevel 上自动应用。
     */
    void setMinimizedGeometry(const QString &appId, struct wl_surface *panel,
                              int x, int y, int w, int h);

    /** 移除某个 app 的 Dock 图标几何信息。 */
    void unsetMinimizedGeometry(const QString &appId);

    // 以下由 Wayland 协议静态回调调用，需为 public
    void bindManagement(wl_registry *registry, uint32_t name, uint32_t version);
    void bindToplevelManager(wl_registry *registry, uint32_t name, uint32_t version);
    void addToplevel(const QString &uuid);
    void removeToplevel(const QString &uuid);
    void setToplevelAppId(const QString &uuid, const QString &appId);

private:
    explicit UkuiWindowManagement(QObject *parent = nullptr);
    ~UkuiWindowManagement() override;

    void ensureInit();
    void applyGeometryToToplevel(const QString &uuid, const QString &appId);
    void applyGeometryToApp(const QString &appKey);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    ukui_window_management *m_management = nullptr;
    kywc_toplevel_manager_v1 *m_toplevelManager = nullptr;
    bool m_inited = false;

    struct Toplevel
    {
        QString appId;
    };
    QHash<QString, Toplevel> m_toplevels;      // uuid -> 信息
    QHash<QString, ukui_window *> m_windows;   // uuid -> ukui_window 资源

    struct Geo
    {
        wl_surface *panel = nullptr;
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };
    QHash<QString, Geo> m_appGeometry;         // 小写 appId -> 最近一次图标几何
};

#endif  // FRAME_WAYLAND_UKUIWINDOWMANAGEMENT_H_
