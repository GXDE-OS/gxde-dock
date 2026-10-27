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

#include "bluetoothplugin.h"
#include "dbus/dbusbluetooth.h"
#include "../../widgets/tipswidget.h"

#include <DDBusSender>

#include <QJsonDocument>

#define PLUGIN_STATE_KEY "enable"

BluetoothPlugin::BluetoothPlugin(QObject *parent)
    : QObject(parent)
{
}

const QString BluetoothPlugin::pluginName() const
{
    return QStringLiteral("bluetooth");
}

const QString BluetoothPlugin::pluginDisplayName() const
{
    return tr("Bluetooth");
}

void BluetoothPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    m_bluetoothInter = new DBusBluetooth(this);
    m_bluetoothStatusWidget = new BluetoothStatusWidget(m_bluetoothInter);

    m_tipsLabel = new TipsWidget;
    m_tipsLabel->setVisible(false);
    m_tipsLabel->setObjectName(QStringLiteral("bluetooth"));

    connect(m_bluetoothInter, &DBusBluetooth::adaptersChanged,
            this, &BluetoothPlugin::refreshPluginItemsVisible);

    refreshPluginItemsVisible();
}

bool BluetoothPlugin::pluginIsDisable()
{
    return !m_proxyInter->getValue(this, PLUGIN_STATE_KEY, true).toBool();
}

void BluetoothPlugin::pluginStateSwitched()
{
    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, pluginIsDisable());
    refreshPluginItemsVisible();
}

void BluetoothPlugin::refreshPluginItemsVisible()
{
    // 仅在有蓝牙适配器、且插件未被禁用时显示
    if (!pluginIsDisable() && m_bluetoothInter && m_bluetoothInter->hasAdapter())
        m_proxyInter->itemAdded(this, BLUETOOTH_KEY);
    else
        m_proxyInter->itemRemoved(this, BLUETOOTH_KEY);
}

QWidget *BluetoothPlugin::itemWidget(const QString &itemKey)
{
    if (itemKey == BLUETOOTH_KEY)
        return m_bluetoothStatusWidget;

    return nullptr;
}

QWidget *BluetoothPlugin::itemTipsWidget(const QString &itemKey)
{
    if (itemKey != BLUETOOTH_KEY)
        return nullptr;

    QString text;
    if (!m_bluetoothInter->isPowered()) {
        text = tr("Turned off");
    } else {
        const QStringList connected = m_bluetoothInter->connectedDevices();
        if (!connected.isEmpty()) {
            QStringList lines;
            for (const QString &name : connected)
                lines << tr("%1 connected").arg(name);
            text = lines.join('\n');
        } else {
            text = tr("Bluetooth");
        }
    }

    m_tipsLabel->setObjectName(itemKey);
    m_tipsLabel->setText(text);
    return m_tipsLabel;
}

QWidget *BluetoothPlugin::itemPopupApplet(const QString &itemKey)
{
    if (itemKey != BLUETOOTH_KEY)
        return nullptr;

    if (!m_bluetoothApplet)
        m_bluetoothApplet = new BluetoothApplet(m_bluetoothInter);

    return m_bluetoothApplet;
}

const QString BluetoothPlugin::itemContextMenu(const QString &itemKey)
{
    if (itemKey != BLUETOOTH_KEY)
        return QString();

    QList<QVariant> items;

    QMap<QString, QVariant> power;
    power["itemId"] = "power";
    power["itemText"] = m_bluetoothInter->isPowered() ? tr("Turn off") : tr("Turn on");
    power["isActive"] = true;
    items.push_back(power);

    QMap<QString, QVariant> settings;
    settings["itemId"] = "settings";
    settings["itemText"] = tr("Bluetooth settings");
    settings["isActive"] = true;
    items.push_back(settings);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void BluetoothPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(itemKey)
    Q_UNUSED(checked)

    if (menuId == "power") {
        m_bluetoothInter->setPowered(!m_bluetoothInter->isPowered());
    } else if (menuId == "settings") {
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QStringLiteral("ShowModule"))
            .arg(QStringLiteral("bluetooth"))
            .call();
    }
}

int BluetoothPlugin::itemSortKey(const QString &itemKey)
{
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(displayMode());
    return m_proxyInter->getValue(this, key, 3).toInt();
}

void BluetoothPlugin::setSortKey(const QString &itemKey, const int order)
{
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(displayMode());
    m_proxyInter->saveValue(this, key, order);
}

void BluetoothPlugin::refreshIcon(const QString &itemKey)
{
    Q_UNUSED(itemKey)
    if (m_bluetoothStatusWidget)
        m_bluetoothStatusWidget->refreshIcon();
}

void BluetoothPlugin::pluginSettingsChanged()
{
    refreshPluginItemsVisible();
}

QVariantList BluetoothPlugin::dockableItems()
{
    QVariantList items;
    QVariantMap item;
    item["key"] = BLUETOOTH_KEY;
    item["name"] = tr("Bluetooth");
    item["docked"] = !pluginIsDisable();
    items << item;
    return items;
}

void BluetoothPlugin::setItemDocked(const QString &key, bool docked)
{
    if (key != BLUETOOTH_KEY)
        return;

    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, docked);
    refreshPluginItemsVisible();
}
