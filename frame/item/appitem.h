/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             listenerri <listenerri@gmail.com>
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

#ifndef APPITEM_H
#define APPITEM_H

#include "dockitem.h"
#include "components/previewcontainer.h"
#include "components/appdrag.h"
#include "dbus/dbusclientmanager.h"
#include "../widgets/tipswidget.h"

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsItemAnimation>
#include <QSet>

#include "dbus/dockentryinterface.h"

using DockEntryInter = DockEntryInterface;

class AppItem : public DockItem
{
    Q_OBJECT

public:
    explicit AppItem(const QDBusObjectPath &entry, QWidget *parent = nullptr,
                     const quint32 windowId = 0, const bool windowSplit = false);
    ~AppItem();

    const QString appId() const;
    const bool isValid() const;

    // 不合并图标（窗口拆分）模式下，每个图标代表一个窗口：
    // windowId 非 0 时该图标只负责这一个窗口，为 0 时代表整个条目
    // （仅用于条目没有任何窗口时，如仅驻留未启动的应用）。
    bool isSplitWindow() const { return m_windowId != 0; }
    quint32 windowId() const { return m_windowId; }
    const WindowInfoMap &windowInfos() const { return m_windowInfos; }

    // 用于最小化动画：取条目对应应用的 desktop file 基名作为 app_id，
    // 以匹配合成器 toplevel 的 app_id。注意 AppItem::appId() 返回的是 Dock
    // 内部条目序号，不能用于此匹配。
    QString animationAppId() const;
    void updateWindowIconGeometries();
    static void setIconBaseSize(const int size, const qreal ratio = 1.0);
    static int iconBaseSize();
    static qreal iconBaseRatio();
    static int itemBaseHeight();
    static int itemBaseWidth();
    void undock();
    QWidget *appDragWidget();
    void setDockInfo(Dock::Position dockPosition, const QRect &dockGeometry);

    inline ItemType itemType() const Q_DECL_OVERRIDE { return App; }
    QSize sizeHint() const override;
    QString getEntryPath() const { return m_entryPath; }

signals:
    void requestActivateWindow(const WId wid) const;
    void requestPreviewWindow(const WId wid) const;
    void requestCancelPreview() const;
    void dragReady(QWidget *dragWidget);
    // 不合并图标模式下条目窗口列表发生变化
    void windowAppeared(const quint32 winId) const;
    void windowClosed(const quint32 winId) const;

private:
    void moveEvent(QMoveEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void paintV23Plate(QPainter &painter);
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void showEvent(QShowEvent *e) override;

    void showHoverTips() Q_DECL_OVERRIDE;
    void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
    const QString contextMenu() const Q_DECL_OVERRIDE;
    QWidget *popupTips() Q_DECL_OVERRIDE;

    void startDrag();
    bool hasAttention() const;

    QPoint appIconPosition() const;

    // 取 Dock 面板（顶层窗口）对应的 wl_surface，用于 ukui_window_management
    // 的 set_minimized_geometry（几何坐标相对该 surface）。
    struct wl_surface *panelSurface() const;

private slots:
    void updateWindowInfos(const WindowInfoMap &info);
    void refershIcon() Q_DECL_OVERRIDE;
    void activeChanged();
    void showPreview();
    void playSwingEffect();
    void stopSwingEffect();
    void checkAttentionEffect();

private:
    TipsWidget *m_appNameTips;
    PreviewContainer *m_appPreviewTips;
    DockEntryInter *m_itemEntryInter;
    QString m_entryPath;

    QGraphicsView *m_swingEffectView;
    QGraphicsItemAnimation *m_itemAnimation;

    DWindowManagerHelper *m_wmHelper;

    QPointer<AppDrag> m_drag;

    bool m_dragging;
    bool m_active;
    int m_retryTimes;

    WindowInfoMap m_windowInfos;
    QString m_id;
    // 不合并图标（窗口拆分）模式
    bool m_windowSplit;
    quint32 m_windowId;
    QSet<quint32> m_knownWindows;
    QPixmap m_appIcon;
    QPixmap m_horizontalIndicator;
    QPixmap m_verticalIndicator;
    QPixmap m_activeHorizontalIndicator;
    QPixmap m_activeVerticalIndicator;

    QTimer *m_updateIconGeometryTimer;
    QTimer *m_retryObtainIconTimer;

    QFutureWatcher<QPixmap> *m_smallWatcher;
    QFutureWatcher<QPixmap> *m_largeWatcher;

    static int IconBaseSize;
    static qreal IconBaseRatio;
    static QPoint MousePressPos;
};

#endif // APPITEM_H
