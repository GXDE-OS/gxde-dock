// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23NETWORKMODULE_H
#define V23NETWORKMODULE_H

#include "v23quickmodule.h"

#include <QPointer>

namespace dde {
namespace network {
class NetworkDevice;
class NetworkModel;
class NetworkWorker;
}
}

class V23ListView;
class QTimer;

// The wired and wireless networks: a switch tile, the state on the dock, and
// the devices with their connections and access points in its page
class V23NetworkModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23NetworkModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("network"); }
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

private:
    enum State {
        NoDevice,
        Disabled,
        Disconnected,
        Connecting,
        Connected,
    };

    struct QuickState {
        QString title;
        QString description;
        QString icon;
        bool active = true;
    };

    State state() const;
    QString stateIcon() const;
    QuickState quickState() const;
    bool anyEnabled() const;
    void setAllEnabled(bool enabled);
    void onDeviceListChanged(const QList<dde::network::NetworkDevice *> &devices);
    void notifyChanged();
    void updateTile();
    void rebuildPages();
    void showConnectionEditor(const QString &devicePath, const QString &apPath, const QString &uuid);

private:
    dde::network::NetworkModel *m_model;
    dde::network::NetworkWorker *m_worker;
    QList<dde::network::NetworkDevice *> m_devices;
    QPointer<V23QuickTile> m_tile;
    QList<QPointer<V23ListView>> m_pages;
    QTimer *m_rebuildTimer;
    QTimer *m_animationTimer;
    QTimer *m_connectivityTimer;
};

#endif // V23NETWORKMODULE_H
