// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23bluetoothmodule.h"
#include "v23dbusobject.h"

#include <DDBusSender>
#include <QDBusObjectPath>
#include <QEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString BluetoothService = QStringLiteral("com.deepin.daemon.Bluetooth");
const QString BluetoothPath = QStringLiteral("/com/deepin/daemon/Bluetooth");

QJsonObject parseObject(const QString &json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

// the device icons of the dde-tray-loader bluetooth plugin, by the icon
// names of BlueZ
QString deviceIcon(const QJsonObject &device)
{
    static const QList<QPair<QString, QString>> icons {
        { "audio-headset", "bluetooth_pheadset" },
        { "audio-headphones", "bluetooth_pheadset" },
        { "audio-card", "bluetooth_clang" },
        { "audio", "bluetooth_clang" },
        { "camera-video", "bluetooth_vidicon" },
        { "camera", "bluetooth_camera" },
        { "input-keyboard", "bluetooth_keyboard" },
        { "input-mouse", "bluetooth_mouse" },
        { "input-tablet", "bluetooth_pad" },
        { "input-gaming", "bluetooth_other" },
        { "phone", "bluetooth_phone" },
        { "computer", "bluetooth_pc" },
        { "printer", "bluetooth_print" },
        { "scanner", "bluetooth_scaner" },
        { "video-display", "bluetooth_tv" },
        { "network", "bluetooth_lan" },
    };

    const QString icon = device.value("Icon").toString();
    for (const auto &pair : icons) {
        if (icon.startsWith(pair.first))
            return pair.second;
    }

    return QStringLiteral("bluetooth_other");
}

QString deviceName(const QJsonObject &device)
{
    const QString alias = device.value("Alias").toString();
    return alias.isEmpty() ? device.value("Name").toString() : alias;
}
}

V23BluetoothModule::V23BluetoothModule(QObject *parent)
    : V23QuickModule(parent)
    , m_bluetooth(new V23DBusObject(QDBusConnection::sessionBus(), BluetoothService, BluetoothPath,
                                    BluetoothService, this))
    , m_rebuildTimer(new QTimer(this))
{
    // the devices report their signal often while discovering
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(300);
    connect(m_rebuildTimer, &QTimer::timeout, this, &V23BluetoothModule::rebuildPages);

    QDBusConnection bus = QDBusConnection::sessionBus();
    const struct {
        const char *signal;
        const char *slot;
    } signalSlots[] = {
        { "AdapterAdded", SLOT(onAdapterAdded(QString)) },
        { "AdapterRemoved", SLOT(onAdapterRemoved(QString)) },
        { "AdapterPropertiesChanged", SLOT(onAdapterChanged(QString)) },
        { "DeviceAdded", SLOT(onDeviceAdded(QString)) },
        { "DeviceRemoved", SLOT(onDeviceRemoved(QString)) },
        { "DevicePropertiesChanged", SLOT(onDeviceChanged(QString)) },
    };
    for (const auto &signalSlot : signalSlots)
        bus.connect(BluetoothService, BluetoothPath, BluetoothService, signalSlot.signal, this, signalSlot.slot);

    connect(m_bluetooth, &V23DBusObject::invalidated, this, [this] {
        m_adapters.clear();
        m_devices.clear();
        notifyChanged(true);
    });
    connect(m_bluetooth, &V23DBusObject::propertiesChanged, this, [this] {
        if (m_adapters.isEmpty())
            loadAdapters();
    });

    loadAdapters();
}

QString V23BluetoothModule::displayName() const
{
    return tr("Bluetooth");
}

bool V23BluetoothModule::isAvailable() const
{
    return !m_adapters.isEmpty();
}

void V23BluetoothModule::loadAdapters()
{
    m_bluetooth->call("GetAdapters", {}, [this](const QVariant &result) {
        m_adapters.clear();
        for (const QJsonValue &value : QJsonDocument::fromJson(result.toString().toUtf8()).array()) {
            const QJsonObject adapter = value.toObject();
            const QString path = adapter.value("Path").toString();
            m_adapters.insert(path, adapter);
            loadDevices(path);
        }
        notifyChanged(true);
    });
}

void V23BluetoothModule::loadDevices(const QString &adapterPath)
{
    m_bluetooth->call("GetDevices", {QVariant::fromValue(QDBusObjectPath(adapterPath))}, [this, adapterPath](const QVariant &result) {
        QMap<QString, QJsonObject> &devices = m_devices[adapterPath];
        devices.clear();
        for (const QJsonValue &value : QJsonDocument::fromJson(result.toString().toUtf8()).array()) {
            const QJsonObject device = value.toObject();
            devices.insert(device.value("Path").toString(), device);
        }
        notifyChanged();
    });
}

void V23BluetoothModule::onAdapterAdded(const QString &json)
{
    const QJsonObject adapter = parseObject(json);
    const QString path = adapter.value("Path").toString();
    m_adapters.insert(path, adapter);
    loadDevices(path);
    notifyChanged(true);
}

void V23BluetoothModule::onAdapterRemoved(const QString &json)
{
    const QString path = parseObject(json).value("Path").toString();
    m_adapters.remove(path);
    m_devices.remove(path);
    notifyChanged(true);
}

void V23BluetoothModule::onAdapterChanged(const QString &json)
{
    const QJsonObject adapter = parseObject(json);
    const QString path = adapter.value("Path").toString();
    if (m_adapters.contains(path)) {
        m_adapters.insert(path, adapter);
        notifyChanged();
    }
}

void V23BluetoothModule::onDeviceAdded(const QString &json)
{
    const QJsonObject device = parseObject(json);
    const QString adapterPath = device.value("AdapterPath").toString();
    if (m_adapters.contains(adapterPath)) {
        m_devices[adapterPath].insert(device.value("Path").toString(), device);
        notifyChanged();
    }
}

void V23BluetoothModule::onDeviceRemoved(const QString &json)
{
    const QJsonObject device = parseObject(json);
    m_devices[device.value("AdapterPath").toString()].remove(device.value("Path").toString());
    notifyChanged();
}

void V23BluetoothModule::onDeviceChanged(const QString &json)
{
    onDeviceAdded(json);
}

void V23BluetoothModule::notifyChanged(bool adaptersChanged)
{
    if (adaptersChanged)
        emit availableChanged();

    updateTile();
    m_rebuildTimer->start();
    emit changed();
}

bool V23BluetoothModule::isPowered() const
{
    for (const QJsonObject &adapter : m_adapters) {
        if (adapter.value("Powered").toBool())
            return true;
    }

    return false;
}

QStringList V23BluetoothModule::connectedDevices() const
{
    QStringList names;
    for (const auto &devices : m_devices) {
        for (const QJsonObject &device : devices) {
            if (device.value("State").toInt() == Connected)
                names << deviceName(device);
        }
    }

    return names;
}

void V23BluetoothModule::setPowered(bool powered)
{
    for (const QString &path : m_adapters.keys())
        m_bluetooth->call("SetAdapterPowered", {QVariant::fromValue(QDBusObjectPath(path)), powered});
}

V23Icon V23BluetoothModule::dockIcon() const
{
    return V23Icon { isPowered() ? QStringLiteral("bluetooth-active-symbolic") : QStringLiteral("bluetooth-disable-symbolic") };
}

QString V23BluetoothModule::tipText() const
{
    if (!isPowered())
        return tr("Turned off");

    QStringList lines;
    for (const QString &name : connectedDevices())
        lines << tr("%1 connected").arg(name);
    if (!lines.isEmpty())
        return lines.join('\n');

    for (const auto &devices : m_devices) {
        for (const QJsonObject &device : devices) {
            if (device.value("State").toInt() == Connecting && device.value("Paired").toBool())
                return tr("Connecting...");
        }
    }

    return tr("Bluetooth");
}

V23QuickModule::MenuItems V23BluetoothModule::menuItems() const
{
    return {
        { QStringLiteral("power"), isPowered() ? tr("Turn off") : tr("Turn on") },
        { QStringLiteral("settings"), tr("Bluetooth settings") },
    };
}

void V23BluetoothModule::invokeMenuItem(const QString &id)
{
    if (id == "power") {
        setPowered(!isPowered());
    } else if (id == "settings") {
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("ShowModule"))
            .arg(QString("bluetooth"))
            .call();
    }
}

void V23BluetoothModule::updateTile()
{
    if (!m_tile)
        return;

    const bool powered = isPowered();
    const QStringList connected = connectedDevices();
    m_tile->setIconActive(powered);
    m_tile->setIcon(powered ? QStringLiteral("bluetooth-active-symbolic") : QStringLiteral("bluetooth-disable-symbolic"));
    if (!powered)
        m_tile->setDescription(tr("Off"));
    else if (!connected.isEmpty())
        m_tile->setDescription(connected.first());
    else
        m_tile->setDescription(tr("On"));
}

QWidget *V23BluetoothModule::createTile(QWidget *parent)
{
    m_tile = new V23QuickTile(V23QuickTile::Large, parent);
    m_tile->setTitle(displayName());
    connect(m_tile, &V23QuickTile::iconClicked, this, [this] {
        setPowered(!isPowered());
    });
    connect(m_tile, &V23QuickTile::clicked, this, &V23BluetoothModule::requestPage);
    updateTile();
    return m_tile;
}

QWidget *V23BluetoothModule::createPage(QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    V23ListView *list = new V23ListView(420, page);
    m_pages << list;
    layout->addWidget(list);

    V23ListItem *settings = new V23ListItem(page);
    settings->setText(tr("Bluetooth settings"));
    settings->setArrow(true);
    layout->addSpacing(V23Quick::PanelSpacing);
    layout->addWidget(settings);
    connect(settings, &V23ListItem::clicked, this, [this] {
        emit requestHidePopup();
        invokeMenuItem("settings");
    });

    // look for devices around each time the page shows
    page->installEventFilter(this);
    rebuildPages();
    return page;
}

bool V23BluetoothModule::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        for (const QJsonObject &adapter : m_adapters) {
            if (adapter.value("Powered").toBool())
                m_bluetooth->call("RequestDiscovery", {QVariant::fromValue(QDBusObjectPath(adapter.value("Path").toString()))});
        }
    }

    return V23QuickModule::eventFilter(watched, event);
}

void V23BluetoothModule::rebuildPages()
{
    for (const QPointer<V23ListView> &list : m_pages) {
        if (!list)
            continue;

        const int scroll = list->verticalScrollBar()->value();
        list->clear();
        QVBoxLayout *layout = list->listLayout();

        QWidget *content = list->widget();
        for (const QJsonObject &adapter : m_adapters) {
            const QString adapterPath = adapter.value("Path").toString();
            const bool powered = adapter.value("Powered").toBool();

            V23SectionHeader *header = new V23SectionHeader(adapter.value("Alias").toString(), true, content);
            header->setChecked(powered);
            layout->addWidget(header);
            connect(header, &V23SectionHeader::toggled, this, [this, adapterPath](bool checked) {
                m_bluetooth->call("SetAdapterPowered", {QVariant::fromValue(QDBusObjectPath(adapterPath)), checked});
            });

            if (!powered)
                continue;

            // the paired devices, then the others that have a name
            for (bool paired : {true, false}) {
                QList<QJsonObject> devices;
                for (const QJsonObject &device : m_devices.value(adapterPath)) {
                    if (device.value("Paired").toBool() == paired && (paired || !device.value("Name").toString().isEmpty()))
                        devices << device;
                }
                if (devices.isEmpty())
                    continue;

                std::stable_sort(devices.begin(), devices.end(), [](const QJsonObject &a, const QJsonObject &b) {
                    return a.value("State").toInt() > b.value("State").toInt();
                });

                V23SectionHeader *title = new V23SectionHeader(paired ? tr("My Devices") : tr("Other Devices"), false, content);
                layout->addWidget(title);

                for (const QJsonObject &device : devices) {
                    const QString devicePath = device.value("Path").toString();
                    const int state = device.value("State").toInt();

                    V23ListItem *item = new V23ListItem(content);
                    item->setText(deviceName(device));
                    item->setIcon(V23Icon { deviceIcon(device) });
                    item->setSelected(state == Connected);
                    item->setBusy(state == Connecting && paired);
                    item->setHoverAction(tr("Disconnect"));
                    layout->addWidget(item);

                    const QVariant path = QVariant::fromValue(QDBusObjectPath(devicePath));
                    connect(item, &V23ListItem::clicked, this, [this, path, state] {
                        if (state != Connected)
                            m_bluetooth->call("ConnectDevice", {path});
                    });
                    connect(item, &V23ListItem::actionClicked, this, [this, path] {
                        m_bluetooth->call("DisconnectDevice", {path});
                    });
                }
            }
        }

        list->verticalScrollBar()->setValue(scroll);
    }
}
