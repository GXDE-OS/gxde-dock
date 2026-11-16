/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#ifndef PERMISSIONMONITORPLUGIN_H
#define PERMISSIONMONITORPLUGIN_H

#include "devicemonitor.h"
#include "monitoritem.h"
#include "pluginsiteminterface.h"

class TipsWidget;
class QWidget;

// 权限监视插件：摄像头或麦克风被其它程序占用时在任务栏显示对应图标。
// 两个图标位于同一 item 内并排显示（完全为一个整体，中间不会插入其它
// 插件），仅占用其中一个设备时只显示对应图标，都被占用时同时显示，
// 都空闲时整个 item 不显示
class PermissionMonitorPlugin : public QObject,
        PluginsItemInterface,
        PluginsItemFactory<PermissionMonitorPlugin>
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterface PluginsItemFactoryInterface)
    Q_PLUGIN_METADATA(IID "com.deepin.dock.PluginsItemInterface" FILE "permission-monitor.json")

public:
    explicit PermissionMonitorPlugin(QObject *parent = nullptr);

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;

    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;

    // 摄像头/麦克风两个图标并排显示，宽度由自身 sizeHint 决定
    PluginSizePolicy pluginSizePolicy() const override { return Custom; }

    bool pluginIsAllowDisable() override { return true; }
    bool pluginIsDisable() override;
    void pluginStateSwitched() override;

    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;

    void displayModeChanged(const Dock::DisplayMode displayMode) override;
    void refreshIcon(const QString &itemKey) override;

private slots:
    void onUsageChanged();

private:
    QString tipsText() const;
    QString tipsTextFor(DeviceMonitor::DeviceType type) const;
    // 根据当前占用状态与插件启用状态同步 item 的显隐
    void updateAllItems();
    void updateItemVisible(bool visible);
    void refreshTips();

    QWidget *m_container;
    MonitorItem *m_cameraItem;
    MonitorItem *m_microphoneItem;
    TipsWidget *m_tipsWidget;
    bool m_itemAdded;
    DeviceMonitor *m_deviceMonitor;
};

#endif // PERMISSIONMONITORPLUGIN_H
