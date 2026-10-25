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

#ifndef DBUSBLUETOOTH_H
#define DBUSBLUETOOTH_H

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QVariant>
#include <functional>

class QDBusInterface;
class QDBusPendingCallWatcher;

// 设备连接状态，来自 BlueZ 的 Device.State
enum BluetoothDeviceState {
    BluetoothUnavailable = 0,
    BluetoothConnecting = 1,
    BluetoothConnected = 2,
};

/*
 * com.deepin.daemon.Bluetooth 的封装：跟踪适配器与设备，
 * 提供开关蓝牙、连接/断开设备等操作，逻辑参考 V23 quick-settings
 * 的 v23bluetoothmodule。
 */
class DBusBluetooth : public QObject
{
    Q_OBJECT

public:
    explicit DBusBluetooth(QObject *parent = nullptr);

    bool hasAdapter() const;
    bool isPowered() const;
    QStringList connectedDevices() const;

    void setPowered(bool powered);
    void connectDevice(const QString &devicePath);
    void disconnectDevice(const QString &devicePath);
    void requestDiscovery(const QString &adapterPath);

    QList<QJsonObject> adapters() const;
    QList<QJsonObject> devices(const QString &adapterPath) const;

signals:
    void adaptersChanged();
    void devicesChanged();

private slots:
    void loadAdapters();

private:
    void loadDevices(const QString &adapterPath);
    void call(const QString &method, const QVariantList &args, std::function<void(const QVariant &)> callback);
    void call(const QString &method, const QVariantList &args);
    void notify(bool adapters);

private:
    QDBusInterface *m_inter = nullptr;
    QMap<QString, QJsonObject> m_adapters;
    QMap<QString, QMap<QString, QJsonObject>> m_devices; // by adapter, by device path
};

#endif // DBUSBLUETOOTH_H
