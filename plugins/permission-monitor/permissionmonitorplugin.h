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

// 权限监视插件：摄像头或麦克风被其它程序占用时在任务栏显示对应图标，
// 两者同时被占用时同时显示两个图标，未被占用时不显示任何图标
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
    DeviceMonitor::DeviceType typeForKey(const QString &itemKey) const;
    QString tipsTextFor(const QString &itemKey) const;
    // 根据当前占用状态与插件启用状态同步两个图标的显隐
    void updateAllItems();
    // 按需增删单个图标（空闲时未添加过 item 即不会显示）
    void updateItemVisible(const QString &itemKey, bool visible);
    void refreshTips();

    MonitorItem *m_cameraItem;
    MonitorItem *m_microphoneItem;
    TipsWidget *m_tipsWidget;
    QString m_currentTipsKey;
    QMap<QString, bool> m_itemAdded;
    DeviceMonitor *m_deviceMonitor;
};

#endif // PERMISSIONMONITORPLUGIN_H
