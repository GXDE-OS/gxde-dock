// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23BLUETOOTHMODULE_H
#define V23BLUETOOTHMODULE_H

#include "v23quickmodule.h"

#include <QJsonObject>
#include <QMap>
#include <QPointer>

class V23DBusObject;
class V23ListView;
class QTimer;

// Bluetooth: a switch tile, the adapters and their devices in its page
class V23BluetoothModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23BluetoothModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("bluetooth"); }
    QString displayName() const override;
    bool isAvailable() const override;

    V23Icon dockIcon() const override;
    QString tipText() const override;
    MenuItems menuItems() const override;
    void invokeMenuItem(const QString &id) override;

    QWidget *createTile(QWidget *parent) override;
    bool hasPage() const override { return true; }
    QWidget *createPage(QWidget *parent) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAdapterAdded(const QString &json);
    void onAdapterRemoved(const QString &json);
    void onAdapterChanged(const QString &json);
    void onDeviceAdded(const QString &json);
    void onDeviceRemoved(const QString &json);
    void onDeviceChanged(const QString &json);

private:
    enum DeviceState {
        Unavailable = 0,
        Connecting = 1,
        Connected = 2,
    };

    bool isPowered() const;
    QStringList connectedDevices() const;
    void setPowered(bool powered);
    void loadAdapters();
    void loadDevices(const QString &adapterPath);
    void notifyChanged(bool adaptersChanged = false);
    void rebuildPages();
    void updateTile();

private:
    V23DBusObject *m_bluetooth;
    QMap<QString, QJsonObject> m_adapters;
    QMap<QString, QMap<QString, QJsonObject>> m_devices; // by adapter, by path
    QPointer<V23QuickTile> m_tile;
    QList<QPointer<V23ListView>> m_pages;
    QTimer *m_rebuildTimer;
};

#endif // V23BLUETOOTHMODULE_H
