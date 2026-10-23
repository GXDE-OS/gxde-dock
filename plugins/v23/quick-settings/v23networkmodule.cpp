// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// The icons follow NetworkPluginHelper::iconPath() of dde-network-core 2.0;
// the devices are driven like the built-in network plugin does.

#include "v23networkmodule.h"

#include <DDBusSender>
#include <NetworkDevice>
#include <NetworkModel>
#include <NetworkWorker>
#include <WiredDevice>
#include <WirelessDevice>
#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollBar>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

using namespace dde::network;

namespace {
bool isConnecting(NetworkDevice::DeviceStatus status)
{
    return status >= NetworkDevice::Prepare && status < NetworkDevice::Activated;
}

// NetManager::StrengthLevelString of dde-network-core
QString strengthLevel(int strength)
{
    if (strength <= 5)
        return QStringLiteral("no");
    if (strength <= 30)
        return QStringLiteral("low");
    if (strength <= 55)
        return QStringLiteral("medium");
    if (strength <= 65)
        return QStringLiteral("high");
    return QStringLiteral("full");
}

QString wirelessIcon(int strength, bool secured = false)
{
    const QString level = strengthLevel(strength);
    // there is no secured icon for no signal
    if (secured && level != QLatin1String("no"))
        return QString("network-wireless-signal-%1-secure-symbolic").arg(level);
    return QString("network-wireless-signal-%1-symbolic").arg(level);
}

QString deviceTitle(NetworkDevice *device, int index, int count)
{
    const bool wireless = device->type() == NetworkDevice::Wireless;
    if (count <= 1)
        return wireless ? V23NetworkModule::tr("Wireless Network") : V23NetworkModule::tr("Wired Network");
    return (wireless ? V23NetworkModule::tr("Wireless Network %1") : V23NetworkModule::tr("Wired Network %1")).arg(index);
}

void showNetworkSettings()
{
    DDBusSender()
        .service("com.deepin.dde.ControlCenter")
        .interface("com.deepin.dde.ControlCenter")
        .path("/com/deepin/dde/ControlCenter")
        .method(QString("ShowModule"))
        .arg(QString("network"))
        .call();
}
}

V23NetworkModule::V23NetworkModule(QObject *parent)
    : V23QuickModule(parent)
    , m_model(new NetworkModel)
    , m_worker(new NetworkWorker(m_model))
    , m_rebuildTimer(new QTimer(this))
    , m_animationTimer(new QTimer(this))
    , m_connectivityTimer(new QTimer(this))
{
    m_model->moveToThread(qApp->thread());
    m_worker->moveToThread(qApp->thread());
    m_model->setParent(this);
    m_worker->setParent(this);

    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(300);
    connect(m_rebuildTimer, &QTimer::timeout, this, &V23NetworkModule::rebuildPages);

    // the icon of a connecting network moves
    m_animationTimer->setInterval(200);
    connect(m_animationTimer, &QTimer::timeout, this, [this] {
        updateTile();
        emit changed();
    });

    // Some networks reach the internet only after a web login, and the
    // connectivity change may never be signalled; the built-in network plugin
    // checks it again the same way
    m_connectivityTimer->setInterval(30 * 1000);
    connect(m_connectivityTimer, &QTimer::timeout, this, [this] {
        if (!m_model->activeConns().isEmpty())
            m_model->checkConnectivity();
    });
    m_connectivityTimer->start();

    connect(m_model, &NetworkModel::deviceListChanged, this, &V23NetworkModule::onDeviceListChanged);
    connect(m_model, &NetworkModel::connectivityChanged, this, &V23NetworkModule::notifyChanged);
    connect(m_model, &NetworkModel::activeConnectionsChanged, this, &V23NetworkModule::notifyChanged);
    onDeviceListChanged(m_model->devices());
}

QString V23NetworkModule::displayName() const
{
    return tr("Network");
}

bool V23NetworkModule::isAvailable() const
{
    return !m_devices.isEmpty();
}

void V23NetworkModule::onDeviceListChanged(const QList<NetworkDevice *> &devices)
{
    const bool wasAvailable = isAvailable();

    for (NetworkDevice *device : devices) {
        if (m_devices.contains(device))
            continue;

        connect(device, &NetworkDevice::enableChanged, this, &V23NetworkModule::notifyChanged);
        connect(device, static_cast<void (NetworkDevice::*)(NetworkDevice::DeviceStatus) const>(&NetworkDevice::statusChanged),
                this, &V23NetworkModule::notifyChanged);
        connect(device, &QObject::destroyed, this, [this, device] {
            m_devices.removeAll(device);
            notifyChanged();
        });

        if (WirelessDevice *wireless = qobject_cast<WirelessDevice *>(device)) {
            connect(wireless, &WirelessDevice::apAdded, this, &V23NetworkModule::notifyChanged);
            connect(wireless, &WirelessDevice::apRemoved, this, &V23NetworkModule::notifyChanged);
            connect(wireless, &WirelessDevice::apInfoChanged, this, &V23NetworkModule::notifyChanged);
            connect(wireless, &WirelessDevice::activeApInfoChanged, this, &V23NetworkModule::notifyChanged);
            connect(wireless, &WirelessDevice::connectionsChanged, this, &V23NetworkModule::notifyChanged);
            connect(wireless, &WirelessDevice::activateAccessPointFailed, this,
                    [this, wireless](const QString &apPath, const QString &uuid) {
                // most likely a password or more settings are needed
                showConnectionEditor(wireless->path(), apPath, uuid);
            });
            m_worker->queryAccessPoints(wireless->path());
            m_worker->requestWirelessScan();
        } else if (WiredDevice *wired = qobject_cast<WiredDevice *>(device)) {
            connect(wired, &WiredDevice::connectionsChanged, this, &V23NetworkModule::notifyChanged);
            connect(wired, &WiredDevice::activeWiredConnectionInfoChanged, this, &V23NetworkModule::notifyChanged);
        }
    }
    m_devices = devices;

    if (wasAvailable != isAvailable())
        emit availableChanged();
    notifyChanged();
}

void V23NetworkModule::notifyChanged()
{
    if (state() == Connecting) {
        if (!m_animationTimer->isActive())
            m_animationTimer->start();
    } else {
        m_animationTimer->stop();
    }

    updateTile();
    m_rebuildTimer->start();
    emit changed();
}

V23NetworkModule::State V23NetworkModule::state() const
{
    if (m_devices.isEmpty())
        return NoDevice;

    bool enabled = false;
    bool connecting = false;
    for (NetworkDevice *device : m_devices) {
        if (!device->enabled())
            continue;

        enabled = true;
        if (device->status() == NetworkDevice::Activated)
            return Connected;
        if (isConnecting(device->status()))
            connecting = true;
    }

    if (connecting)
        return Connecting;
    return enabled ? Disconnected : Disabled;
}

bool V23NetworkModule::anyEnabled() const
{
    for (NetworkDevice *device : m_devices) {
        if (device->enabled())
            return true;
    }

    return false;
}

void V23NetworkModule::setAllEnabled(bool enabled)
{
    for (NetworkDevice *device : m_devices)
        m_worker->setDeviceEnable(device->path(), enabled);
}

// NetStatus::updateNetworkIcon() of dde-network-core
QString V23NetworkModule::stateIcon() const
{
    const bool full = NetworkModel::connectivity() == Connectivity::Full;
    bool hasWireless = false;
    bool wirelessEnabled = false;
    bool cablePlugged = false;
    NetworkDevice *connecting = nullptr;

    // a wired connection wins over a wireless one
    for (NetworkDevice *device : m_devices) {
        if (device->type() == NetworkDevice::Wired && device->enabled() && device->status() == NetworkDevice::Activated)
            return full ? QStringLiteral("network-online-symbolic") : QStringLiteral("network-offline-symbolic");
    }

    for (NetworkDevice *device : m_devices) {
        const bool wireless = device->type() == NetworkDevice::Wireless;
        hasWireless |= wireless;
        if (!device->enabled())
            continue;

        if (wireless) {
            wirelessEnabled = true;
            if (device->status() == NetworkDevice::Activated) {
                if (!full)
                    return QStringLiteral("network-wireless-offline-signal-full-symbolic");
                return wirelessIcon(static_cast<WirelessDevice *>(device)->activeApStrength());
            }
        } else if (device->status() > NetworkDevice::Unavailable) {
            cablePlugged = true;
        }

        if (!connecting && isConnecting(device->status()))
            connecting = device;
    }

    if (connecting) {
        if (connecting->type() == NetworkDevice::Wireless)
            return wirelessIcon(QTime::currentTime().msec() / 10 % 100);
        return QString("network-wired-symbolic-connecting%1").arg(QTime::currentTime().msec() / 200 % 5 + 1);
    }

    if (!anyEnabled())
        return hasWireless ? QStringLiteral("network-wireless-disabled-symbolic") : QStringLiteral("network-disabled-symbolic");
    if (wirelessEnabled)
        return QStringLiteral("network-wireless-disconnect");
    return cablePlugged ? QStringLiteral("network-none-symbolic") : QStringLiteral("network-error-symbolic");
}

V23Icon V23NetworkModule::dockIcon() const
{
    V23Icon icon { stateIcon() };
    // the red cross of a missing cable stays red
    if (icon.name == QLatin1String("network-error-symbolic"))
        icon.mode = V23Quick::KeepColors;
    return icon;
}

// NetStatus::updateQuick() of dde-network-core: the tile tells about the
// wireless network if there is one, otherwise about the wired one
V23NetworkModule::QuickState V23NetworkModule::quickState() const
{
    NetworkDevice *wireless = nullptr;
    NetworkDevice *wired = nullptr;
    for (NetworkDevice *device : m_devices) {
        if (device->type() == NetworkDevice::Wireless && !wireless)
            wireless = device;
        else if (device->type() == NetworkDevice::Wired && !wired)
            wired = device;
    }

    const bool full = NetworkModel::connectivity() == Connectivity::Full;
    QuickState quick;
    if (NetworkDevice *device = wireless ? wireless : wired) {
        const bool isWireless = device == wireless;
        const NetworkDevice::DeviceStatus status = device->status();
        quick.title = isWireless ? tr("Wireless Network") : tr("Wired Network");
        if (!device->enabled()) {
            quick.active = false;
            quick.description = tr("Off");
            quick.icon = isWireless ? QStringLiteral("network-wireless-disabled-symbolic") : QStringLiteral("network-disabled-symbolic");
        } else if (isConnecting(status)) {
            quick.description = tr("Connecting");
            quick.icon = isWireless ? wirelessIcon(QTime::currentTime().msec() / 10 % 100)
                                    : QString("network-wired-symbolic-connecting%1").arg(QTime::currentTime().msec() / 200 % 5 + 1);
        } else if (status == NetworkDevice::Activated && !full) {
            quick.description = tr("Connected but no Internet access");
            quick.icon = isWireless ? QStringLiteral("network-wireless-offline-signal-full-symbolic") : QStringLiteral("network-offline-symbolic");
        } else if (status == NetworkDevice::Activated) {
            const QString name = isWireless ? static_cast<WirelessDevice *>(device)->activeApSsid()
                                            : static_cast<WiredDevice *>(device)->activeWiredConnName();
            quick.description = name.isEmpty() ? tr("Connected") : name;
            quick.icon = isWireless ? QStringLiteral("network-wireless-signal-full-symbolic") : QStringLiteral("network-online-symbolic");
        } else if (!isWireless && status <= NetworkDevice::Unavailable) {
            quick.description = tr("Network cable unplugged");
            quick.icon = QStringLiteral("network-error-symbolic");
        } else {
            quick.description = tr("Not connected");
            quick.icon = isWireless ? QStringLiteral("network-wireless-disconnect") : QStringLiteral("network-none-symbolic");
        }
    } else {
        quick.title = tr("Network");
        quick.description = tr("Not connected");
        quick.icon = QStringLiteral("network-error-symbolic");
    }

    return quick;
}

QString V23NetworkModule::tipText() const
{
    if (m_devices.isEmpty())
        return tr("No Network");

    QStringList lines;
    const bool full = NetworkModel::connectivity() == Connectivity::Full;
    for (NetworkDevice *device : m_devices) {
        QString line = device->statusStringDetail();
        if (full && device->status() == NetworkDevice::Activated) {
            QJsonObject info;
            if (WiredDevice *wired = qobject_cast<WiredDevice *>(device))
                info = wired->activeWiredConnectionInfo();
            else if (WirelessDevice *wireless = qobject_cast<WirelessDevice *>(device))
                info = wireless->activeWirelessConnectionInfo();

            const QString address = info.value("Ip4").toObject().value("Address").toString();
            if (!address.isEmpty()) {
                line = device->type() == NetworkDevice::Wired ? tr("Wired connection: %1").arg(address)
                                                              : tr("Wireless connection: %1").arg(address);
            }
        }
        lines << line;
    }

    return lines.join('\n');
}

V23QuickModule::MenuItems V23NetworkModule::menuItems() const
{
    return {
        { QStringLiteral("enable"), anyEnabled() ? tr("Disable network") : tr("Enable network") },
        { QStringLiteral("settings"), tr("Network settings") },
    };
}

void V23NetworkModule::invokeMenuItem(const QString &id)
{
    if (id == "enable")
        setAllEnabled(!anyEnabled());
    else if (id == "settings")
        showNetworkSettings();
}

void V23NetworkModule::updateTile()
{
    if (!m_tile)
        return;

    const QuickState quick = quickState();
    m_tile->setTitle(quick.title);
    m_tile->setIcon(quick.icon, quick.icon == QLatin1String("network-error-symbolic") ? V23Quick::KeepColors : V23Quick::Mask);
    m_tile->setIconActive(quick.active);
    m_tile->setDescription(quick.description);
}

QWidget *V23NetworkModule::createTile(QWidget *parent)
{
    m_tile = new V23QuickTile(V23QuickTile::Large, parent);
    connect(m_tile, &V23QuickTile::iconClicked, this, [this] {
        setAllEnabled(!anyEnabled());
    });
    connect(m_tile, &V23QuickTile::clicked, this, &V23NetworkModule::requestPage);
    updateTile();
    return m_tile;
}

QWidget *V23NetworkModule::createPage(QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    V23ListView *list = new V23ListView(420, page);
    m_pages << list;
    layout->addWidget(list);

    V23ListItem *settings = new V23ListItem(page);
    settings->setText(tr("Network settings"));
    settings->setArrow(true);
    layout->addSpacing(V23Quick::PanelSpacing);
    layout->addWidget(settings);
    connect(settings, &V23ListItem::clicked, this, [this] {
        emit requestHidePopup();
        showNetworkSettings();
    });

    // look for access points each time the page shows
    page->installEventFilter(this);
    rebuildPages();
    return page;
}

bool V23NetworkModule::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show)
        m_worker->requestWirelessScan();

    return V23QuickModule::eventFilter(watched, event);
}

void V23NetworkModule::showConnectionEditor(const QString &devicePath, const QString &apPath, const QString &uuid)
{
    QJsonObject data;
    data.insert("conn-type", "wireless");
    data.insert("device-path", devicePath);
    data.insert("conn-uuid", uuid);
    data.insert("ap-path", apPath);

    emit requestHidePopup();
    DDBusSender()
        .service("com.deepin.dde.ControlCenter")
        .interface("com.deepin.dde.ControlCenter")
        .path("/com/deepin/dde/ControlCenter")
        .method("ShowPage")
        .arg(QString("network"))
        .arg(QString(QJsonDocument(data).toJson()))
        .call();
}

void V23NetworkModule::rebuildPages()
{
    int wiredCount = 0;
    int wirelessCount = 0;
    for (NetworkDevice *device : m_devices)
        (device->type() == NetworkDevice::Wireless ? wirelessCount : wiredCount)++;

    for (const QPointer<V23ListView> &list : m_pages) {
        if (!list)
            continue;

        const int scroll = list->verticalScrollBar()->value();
        list->clear();
        QVBoxLayout *layout = list->listLayout();

        QWidget *content = list->widget();
        int wiredIndex = 0;
        int wirelessIndex = 0;
        for (NetworkDevice *device : m_devices) {
            const bool wireless = device->type() == NetworkDevice::Wireless;
            const QString devicePath = device->path();
            const NetworkDevice::DeviceStatus status = device->status();

            V23SectionHeader *header = new V23SectionHeader(
                deviceTitle(device, wireless ? ++wirelessIndex : ++wiredIndex, wireless ? wirelessCount : wiredCount),
                true, content);
            header->setChecked(device->enabled());
            layout->addWidget(header);
            connect(header, &V23SectionHeader::toggled, this, [this, devicePath](bool checked) {
                m_worker->setDeviceEnable(devicePath, checked);
            });

            if (!device->enabled())
                continue;

            if (WiredDevice *wired = qobject_cast<WiredDevice *>(device)) {
                if (status <= NetworkDevice::Unavailable) {
                    V23ListItem *item = new V23ListItem(content);
                    item->setIcon(V23Icon { QStringLiteral("network-error-symbolic"), QSize(16, 16), V23Quick::KeepColors });
                    item->setText(tr("Network cable unplugged"));
                    item->setEnabled(false);
                    layout->addWidget(item);
                    continue;
                }

                const QString activeUuid = wired->activeWiredConnUuid();
                for (const QJsonObject &connection : wired->connections()) {
                    const QString uuid = connection.value("Uuid").toString();
                    const bool active = !activeUuid.isEmpty() && uuid == activeUuid;

                    V23ListItem *item = new V23ListItem(content);
                    item->setIcon(V23Icon { QStringLiteral("network-online-symbolic") });
                    item->setText(connection.value("Id").toString());
                    item->setSelected(active && status == NetworkDevice::Activated);
                    item->setBusy(active && isConnecting(status));
                    item->setHoverAction(tr("Disconnect"));
                    layout->addWidget(item);

                    connect(item, &V23ListItem::clicked, this, [this, item, devicePath, uuid] {
                        // connected already, the hover button disconnects it
                        if (!item->isSelected())
                            m_worker->activateConnection(devicePath, uuid);
                    });
                    connect(item, &V23ListItem::actionClicked, this, [this, devicePath] {
                        m_worker->disconnectDevice(devicePath);
                    });
                }
                continue;
            }

            WirelessDevice *wirelessDevice = static_cast<WirelessDevice *>(device);
            const QString activeSsid = wirelessDevice->activeApSsid();

            // one entry a network name, its strongest access point
            QMap<QString, QJsonObject> accessPoints;
            for (const QJsonValue &value : wirelessDevice->apList()) {
                const QJsonObject ap = value.toObject();
                const QString ssid = ap.value("Ssid").toString();
                if (ssid.isEmpty())
                    continue;
                if (!accessPoints.contains(ssid) || ap.value("Strength").toInt() > accessPoints[ssid].value("Strength").toInt())
                    accessPoints.insert(ssid, ap);
            }

            QList<QJsonObject> sorted = accessPoints.values();
            std::stable_sort(sorted.begin(), sorted.end(), [&activeSsid](const QJsonObject &a, const QJsonObject &b) {
                const bool aActive = a.value("Ssid").toString() == activeSsid;
                const bool bActive = b.value("Ssid").toString() == activeSsid;
                if (aActive != bActive)
                    return aActive;
                return a.value("Strength").toInt() > b.value("Strength").toInt();
            });

            for (const QJsonObject &ap : sorted) {
                const QString ssid = ap.value("Ssid").toString();
                const QString apPath = ap.value("Path").toString();
                const bool active = !activeSsid.isEmpty() && ssid == activeSsid;

                V23ListItem *item = new V23ListItem(content);
                item->setIcon(V23Icon { wirelessIcon(ap.value("Strength").toInt(), ap.value("Secured").toBool()) });
                item->setText(ssid);
                item->setSelected(active && status == NetworkDevice::Activated);
                item->setBusy(active && isConnecting(status));
                item->setHoverAction(tr("Disconnect"));
                layout->addWidget(item);

                connect(item, &V23ListItem::clicked, this, [this, item, wirelessDevice, devicePath, apPath, ssid] {
                    if (item->isSelected())
                        return;

                    QString uuid;
                    for (const QJsonObject &connection : wirelessDevice->connections()) {
                        if (connection.value("Ssid").toString() == ssid) {
                            uuid = connection.value("Uuid").toString();
                            if (!uuid.isEmpty())
                                break;
                        }
                    }
                    m_worker->activateAccessPoint(devicePath, apPath, uuid);
                });
                connect(item, &V23ListItem::actionClicked, this, [this, devicePath] {
                    m_worker->disconnectDevice(devicePath);
                });
            }
        }

        list->verticalScrollBar()->setValue(scroll);
    }
}
