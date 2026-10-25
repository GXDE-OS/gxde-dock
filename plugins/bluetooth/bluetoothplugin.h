/*
 * Copyright (C) 2026 GXDE OS
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
 */

#ifndef BLUETOOTHPLUGIN_H
#define BLUETOOTHPLUGIN_H

#include "pluginsiteminterface.h"
#include "bluetoothstatuswidget.h"
#include "bluetoothapplet.h"

class TipsWidget;

// 高效/时尚模式下的蓝牙插件：图标 + 设备列表面板 + 右键菜单，
// 逻辑参考 V23 quick-settings 的 v23bluetoothmodule。
class BluetoothPlugin : public QObject, PluginsItemInterface, PluginsItemFactory<BluetoothPlugin>
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterface PluginsItemFactoryInterface)
    Q_PLUGIN_METADATA(IID "com.deepin.dock.PluginsItemInterface" FILE "bluetooth.json")

public:
    explicit BluetoothPlugin(QObject *parent = nullptr);

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;
    void pluginStateSwitched() override;
    bool pluginIsAllowDisable() override { return true; }
    bool pluginIsDisable() override;

    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;
    QWidget *itemPopupApplet(const QString &itemKey) override;
    const QString itemContextMenu(const QString &itemKey) override;
    void invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked) override;
    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;
    void refreshIcon(const QString &itemKey) override;
    void pluginSettingsChanged() override;

private:
    void refreshPluginItemsVisible();

private:
    DBusBluetooth *m_bluetoothInter = nullptr;
    BluetoothStatusWidget *m_bluetoothStatusWidget = nullptr;
    BluetoothApplet *m_bluetoothApplet = nullptr;
    TipsWidget *m_tipsLabel = nullptr;
};

#endif // BLUETOOTHPLUGIN_H
