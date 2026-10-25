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

#include "dbusbluetooth.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QJsonArray>
#include <QJsonDocument>

namespace {
const QString BluetoothService = QStringLiteral("com.deepin.daemon.Bluetooth");
const QString BluetoothPath = QStringLiteral("/com/deepin/daemon/Bluetooth");
}

DBusBluetooth::DBusBluetooth(QObject *parent)
    : QObject(parent)
    , m_inter(new QDBusInterface(BluetoothService, BluetoothPath, BluetoothService,
                                 QDBusConnection::sessionBus(), this))
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const struct {
        const char *signal;
        const char *slot;
    } signalSlots[] = {
        { "AdapterAdded", SLOT(loadAdapters()) },
        { "AdapterRemoved", SLOT(loadAdapters()) },
        { "AdapterPropertiesChanged", SLOT(loadAdapters()) },
        { "DeviceAdded", SLOT(loadAdapters()) },
        { "DeviceRemoved", SLOT(loadAdapters()) },
        { "DevicePropertiesChanged", SLOT(loadAdapters()) },
    };
    // 简化处理：任何适配器/设备变化都重新加载数据
    for (const auto &signalSlot : signalSlots) {
        bus.connect(BluetoothService, BluetoothPath, BluetoothService,
                    signalSlot.signal, this, signalSlot.slot);
    }

    loadAdapters();
}

bool DBusBluetooth::hasAdapter() const
{
    return !m_adapters.isEmpty();
}

bool DBusBluetooth::isPowered() const
{
    for (const QJsonObject &adapter : m_adapters) {
        if (adapter.value("Powered").toBool())
            return true;
    }
    return false;
}

QStringList DBusBluetooth::connectedDevices() const
{
    QStringList names;
    for (const auto &devices : m_devices) {
        for (const QJsonObject &device : devices) {
            if (device.value("State").toInt() == BluetoothConnected) {
                const QString alias = device.value("Alias").toString();
                names << (alias.isEmpty() ? device.value("Name").toString() : alias);
            }
        }
    }
    return names;
}

QList<QJsonObject> DBusBluetooth::adapters() const
{
    return m_adapters.values();
}

QList<QJsonObject> DBusBluetooth::devices(const QString &adapterPath) const
{
    return m_devices.value(adapterPath).values();
}

void DBusBluetooth::setPowered(bool powered)
{
    for (const QString &path : m_adapters.keys())
        call("SetAdapterPowered", { QVariant::fromValue(QDBusObjectPath(path)), powered });
    if (!powered)
        return;
    loadAdapters();
}

void DBusBluetooth::connectDevice(const QString &devicePath)
{
    call("ConnectDevice", { QVariant::fromValue(QDBusObjectPath(devicePath)) });
}

void DBusBluetooth::disconnectDevice(const QString &devicePath)
{
    call("DisconnectDevice", { QVariant::fromValue(QDBusObjectPath(devicePath)) });
}

void DBusBluetooth::requestDiscovery(const QString &adapterPath)
{
    call("RequestDiscovery", { QVariant::fromValue(QDBusObjectPath(adapterPath)) });
}

void DBusBluetooth::loadAdapters()
{
    call("GetAdapters", {}, [this](const QVariant &result) {
        QMap<QString, QJsonObject> adapters;
        for (const QJsonValue &value : QJsonDocument::fromJson(result.toString().toUtf8()).array()) {
            const QJsonObject adapter = value.toObject();
            adapters.insert(adapter.value("Path").toString(), adapter);
        }
        const bool changed = adapters.keys() != m_adapters.keys();
        m_adapters = adapters;
        for (const QString &path : m_adapters.keys())
            loadDevices(path);
        notify(changed || m_devices.isEmpty());
    });
}

void DBusBluetooth::loadDevices(const QString &adapterPath)
{
    call("GetDevices", { QVariant::fromValue(QDBusObjectPath(adapterPath)) },
         [this, adapterPath](const QVariant &result) {
        QMap<QString, QJsonObject> &devices = m_devices[adapterPath];
        devices.clear();
        for (const QJsonValue &value : QJsonDocument::fromJson(result.toString().toUtf8()).array()) {
            const QJsonObject device = value.toObject();
            devices.insert(device.value("Path").toString(), device);
        }
        notify(false);
    });
}

void DBusBluetooth::notify(bool adapters)
{
    if (adapters)
        emit adaptersChanged();
    emit devicesChanged();
}

void DBusBluetooth::call(const QString &method, const QVariantList &args,
                         std::function<void(const QVariant &)> callback)
{
    QDBusPendingCall pending = m_inter->asyncCallWithArgumentList(method, args);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [watcher, callback](QDBusPendingCallWatcher *w) {
        const QDBusMessage msg = w->reply();
        const QVariantList arguments = msg.arguments();
        callback(arguments.isEmpty() ? QVariant() : arguments.first());
        watcher->deleteLater();
    });
}

void DBusBluetooth::call(const QString &method, const QVariantList &args)
{
    m_inter->asyncCallWithArgumentList(method, args);
}
