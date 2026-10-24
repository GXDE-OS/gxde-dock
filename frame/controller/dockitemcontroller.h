/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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

#ifndef DOCKITEMCONTROLLER_H
#define DOCKITEMCONTROLLER_H

#include <QObject>
#include <QHash>

#include <types/windowinfomap.h>

#include "dockpluginscontroller.h"
#include "pluginsiteminterface.h"
#include "item/dockitem.h"
#include "item/stretchitem.h"
#include "item/appitem.h"
#include "item/placeholderitem.h"
#include "item/containeritem.h"
#include "item/v23collapseitem.h"
#include "item/v23searchitem.h"
#include "dbus/dockdaemoninterface.h"

using DBusDock = DockDaemonInterface;

class QScreen;

class DockItemController : public QObject
{
    Q_OBJECT

public:
    static DockItemController *instanceForScreen(QScreen *screen, QObject *parent = nullptr);
    static DockItemController *instance(QObject *parent = nullptr);
    static QList<DockItemController *> instances();

    QScreen *screen() const;

    const QList<QPointer<DockItem> > itemList() const;
    const QList<PluginsItemInterface *> pluginList() const;
    QObject *pluginObject(PluginsItemInterface *plugin) const;
    bool appIsOnDock(const QString &appDesktop) const;
    bool itemIsInContainer(DockItem * const item) const;
    void setDropping(const bool dropping);
    void startLoadPlugins() const;
    void reloadAppItems();
    bool v23ItemHidden(DockItem * const item) const;

signals:
    void itemInserted(const int index, DockItem *item) const;
    void itemRemoved(DockItem *item) const;
    void itemMoved(DockItem *item, const int index) const;
    void itemManaged(DockItem *item) const;
    void itemUpdated(DockItem *item) const;
    void fashionTraySizeChanged(const QSize &traySize) const;
    // V23 mode: the tray icons to show changed
    void v23TrayChanged(const bool animate) const;

public slots:
    void refershItemsIcon();
    void sortPluginItems();
    void updatePluginsItemOrderKey();
    void itemMove(DockItem * const moveItem, DockItem * const replaceItem);
    void itemDroppedIntoContainer(DockItem * const item);
    void itemDragOutFromContainer(DockItem * const item);
    void placeholderItemAdded(PlaceholderItem *item, DockItem *position);
    void placeholderItemDocked(const QString &appDesktop, DockItem *position);
    void placeholderItemRemoved(PlaceholderItem *item);
    void refreshFSTItemSpliterVisible();
    void toggleV23Collapsed();

private:
    explicit DockItemController(QScreen *screen, QObject *parent = nullptr);
    AppItem* createAppItem(const QDBusObjectPath& path, const quint32 windowId = 0);
    void appItemAdded(const QDBusObjectPath &path, const int index);
    void appItemRemoved(const QString &appId);
    void appItemRemoved(AppItem *appItem);
    void syncAppItems(const QList<QDBusObjectPath>& entries);
    void pluginItemInserted(PluginsItem *item);
    void pluginItemRemoved(PluginsItem *item);
    // 不合并图标（窗口拆分）辅助函数
    bool windowSplitEnabled() const;
    WindowInfoMap entryWindowInfos(const QString &entryPath) const;
    int appAreaInsertIndex(const int entryIndex) const;
    int insertAppItemsForEntry(const QDBusObjectPath &path, const int insertIndex);
    AppItem *findSplitItem(const QString &entryPath, const quint32 windowId) const;
    void removeEntryPlaceholder(const QString &entryPath);
    void refreshContainerItems();
    bool isV23FixedItem(DockItem * const item) const;
    qreal v23SortKey(DockItem * const item) const;
    int v23TrayStart() const;
    int v23InsertIndex(DockItem * const item) const;
    void ensureV23CollapseItem();
    void updateV23SearchItem();
    int appAreaStart() const;
    void sortV23TrayItems();
    void saveV23TraySettings() const;

private slots:
    // 系统休眠唤醒后主动重新同步窗口/条目，修复休眠后状态错乱
    void connectToLoginManager();
    void onPrepareForSleep(bool active);
    // 不合并图标模式下条目窗口列表变化
    void onSplitWindowAppeared(const quint32 winId);
    void onSplitWindowClosed(const quint32 winId);


private:
    QScreen *m_screen;
    QTimer *m_updatePluginsOrderTimer;
    DBusDock *m_appInter;
    DockPluginsController *m_pluginsInter;
    StretchItem *m_placeholderItem;
    ContainerItem *m_containerItem;
    V23CollapseItem *m_collapseItem;
    V23SearchItem *m_searchItem;
    bool m_v23Collapsed;
    int m_v23CollapseKey;

    static QHash<QScreen *, DockItemController *> INSTANCES;

    QList<QPointer<DockItem>> m_itemList;
};

#endif // DOCKITEMCONTROLLER_H
