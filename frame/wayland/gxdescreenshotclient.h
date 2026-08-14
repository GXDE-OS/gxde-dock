/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FRAME_WAYLAND_GXDESCREENSHOTCLIENT_H_
#define FRAME_WAYLAND_GXDESCREENSHOTCLIENT_H_

#include <QObject>
#include <QImage>
#include <QHash>
#include <QString>

struct wl_display;
struct wl_registry;
struct gxde_screenshot_manager_v1;
struct kywc_toplevel_manager_v1;
struct kywc_toplevel_v1;

class GxdeScreenshotClient : public QObject
{
    Q_OBJECT

public:
    explicit GxdeScreenshotClient(QObject *parent = nullptr);
    ~GxdeScreenshotClient() override;

    static GxdeScreenshotClient *instance();

    // 合成器是否广播了 gxde_screenshot_manager_v1（决定了预览是否走新协议）
    bool available() const;

    // 触发一次 roundtrip，刷新 toplevel 列表（供匹配 uuid 前调用）
    void refreshToplevels();

    // 按 app_id + 窗口标题 匹配窗口 uuid；找不到返回空串。
    // 匹配策略: app_id(忽略大小写) + 标题精确匹配；失败则退化为标题唯一匹配。
    QString findWindowUuid(const QString &appId, const QString &title) const;

    // 抓取窗口缩略图（阻塞调用，合成器一般在一两帧内回传，超时约 1s）。
    // appId 为 dock 条目解析出的 app_id（如 "google-chrome"），title 为窗口标题。
    // maxWidth/maxHeight 为缩略图最大尺寸（合成器按比例缩放）。
    // 成功返回 true 并把图像写入 out，失败返回 false（调用方应回退旧路径）。
    bool captureWindowThumbnail(const QString &appId, const QString &title,
                                quint32 maxWidth, quint32 maxHeight, QImage *out);

    // 以下由协议静态回调使用
    void bindScreenshotManager(wl_registry *registry, quint32 name);
    void bindToplevelManager(wl_registry *registry, quint32 name);
    void handleGlobalRemove(quint32 name);
    void onToplevelManagerFinished();
    void addToplevel(const QString &uuid);
    void removeToplevel(const QString &uuid);
    void setToplevelTitle(const QString &uuid, const QString &title);
    void setToplevelAppId(const QString &uuid, const QString &appId);

private:
    void ensureInit();
    void cleanup();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    gxde_screenshot_manager_v1 *m_manager = nullptr;
    kywc_toplevel_manager_v1 *m_toplevelManager = nullptr;
    bool m_inited = false;

    struct ToplevelInfo
    {
        QString appId;
        QString title;
    };
    QHash<QString, ToplevelInfo> m_toplevels;
};

#endif  // FRAME_WAYLAND_GXDESCREENSHOTCLIENT_H_
