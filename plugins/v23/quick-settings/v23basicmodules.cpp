// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23basicmodules.h"
#include "v23dbusobject.h"

#include <DDBusSender>
#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace {
const QString WlcomScreenService = QStringLiteral("top.gxde.Wlcom.Screen");

const QString ControlCenterCommand = QStringLiteral(
    "dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter "
    "com.deepin.dde.ControlCenter.ShowModule \"string:%1\"");

void showControlCenterModule(const QString &module)
{
    DDBusSender()
        .service("com.deepin.dde.ControlCenter")
        .interface("com.deepin.dde.ControlCenter")
        .path("/com/deepin/dde/ControlCenter")
        .method(QString("ShowModule"))
        .arg(module)
        .call();
}

template<typename T>
QMap<QString, T> toMap(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>())
        return qdbus_cast<QMap<QString, T>>(value.value<QDBusArgument>());

    QMap<QString, T> map;
    const QVariantMap variantMap = value.toMap();
    for (auto it = variantMap.constBegin(); it != variantMap.constEnd(); ++it)
        map.insert(it.key(), it.value().value<T>());
    return map;
}

// the battery states of com.deepin.daemon.Power
enum BatteryState {
    Unknown = 0,
    Charging = 1,
    Discharging = 2,
    NotCharging = 3,
    FullyCharged = 4,
};
}

V23PowerModule::V23PowerModule(QObject *parent)
    : V23QuickModule(parent)
    , m_power(new V23DBusObject(QDBusConnection::sessionBus(), "com.deepin.daemon.Power",
                                "/com/deepin/daemon/Power", "com.deepin.daemon.Power", this))
    , m_systemPower(new V23DBusObject(QDBusConnection::systemBus(), "com.deepin.system.Power",
                                      "/com/deepin/system/Power", "com.deepin.system.Power", this))
    , m_preChargeTimer(new QTimer(this))
    , m_available(false)
{
    // the time to full is not known yet in the first seconds of charging
    m_preChargeTimer->setInterval(20 * 1000);
    m_preChargeTimer->setSingleShot(true);
    connect(m_preChargeTimer, &QTimer::timeout, this, &V23PowerModule::changed);

    connect(m_power, &V23DBusObject::propertiesChanged, this, [this](const QStringList &names) {
        if (names.contains("OnBattery") && plugged())
            m_preChargeTimer->start();

        const bool available = toMap<bool>(m_power->property("BatteryIsPresent")).value("Display");
        if (available != m_available) {
            m_available = available;
            emit availableChanged();
        }
        emit changed();
    });
    connect(m_systemPower, &V23DBusObject::propertiesChanged, this, &V23PowerModule::changed);
}

QString V23PowerModule::displayName() const
{
    return tr("Battery");
}

bool V23PowerModule::isAvailable() const
{
    return m_available;
}

int V23PowerModule::percentage() const
{
    const double value = toMap<double>(m_power->property("BatteryPercentage")).value("Display");
    return int(std::round(qBound(0.0, value, 100.0)));
}

uint V23PowerModule::batteryState() const
{
    return toMap<uint>(m_power->property("BatteryState")).value("Display");
}

bool V23PowerModule::plugged() const
{
    return !m_power->property("OnBattery").toBool();
}

V23Icon V23PowerModule::dockIcon() const
{
    // 0-5%, 6-10%, 11-20%, ... 91-100%
    const int value = percentage();
    const int level = value <= 5 ? 0 : (value <= 10 ? 10 : qMin(100, (value + 9) / 10 * 10));

    const QString state = plugged() ? QStringLiteral("plugged-symbolic") : QStringLiteral("symbolic");

    V23Icon icon;
    icon.mode = V23Quick::KeepColors;
    if (batteryState() == FullyCharged && plugged())
        icon.name = QStringLiteral("battery-full-charged-symbolic");
    else
        icon.name = QString("battery-%1-%2").arg(level, 3, 10, QChar('0')).arg(state);
    return icon;
}

QString V23PowerModule::tipText() const
{
    const QString value = QString("%1%").arg(percentage());
    const uint state = batteryState();

    if (m_preChargeTimer->isActive())
        return tr("Capacity %1 ...").arg(value);

    auto splitTime = [](qulonglong seconds, uint &hour, uint &min) {
        hour = seconds / 3600;
        min = (seconds % 3600 + 59) / 60;
    };

    uint hour = 0;
    uint min = 0;
    if (state == Discharging || state == NotCharging || state == Unknown) {
        splitTime(m_systemPower->property("BatteryTimeToEmpty").toULongLong(), hour, min);
        if (hour == 0 && min == 0)
            return tr("Capacity %1").arg(value);
        if (hour == 0)
            return tr("Capacity %1, %2 min remaining").arg(value).arg(min);
        return tr("Capacity %1, %2 hr %3 min remaining").arg(value).arg(hour).arg(min);
    }

    if (state == FullyCharged || percentage() == 100)
        return tr("Capacity %1, fully charged").arg(value);

    const qulonglong timeToFull = m_systemPower->property("BatteryTimeToFull").toULongLong();
    if (timeToFull == 0)
        return tr("Capacity %1 ...").arg(value);

    splitTime(timeToFull, hour, min);
    if (hour == 0)
        return tr("Charging %1, %2 min until full").arg(value).arg(min);
    return tr("Charging %1, %2 hr %3 min until full").arg(value).arg(hour).arg(min);
}

QString V23PowerModule::dockCommand() const
{
    return ControlCenterCommand.arg("power");
}

V23QuickModule::MenuItems V23PowerModule::menuItems() const
{
    return { { QStringLiteral("settings"), tr("Power settings") } };
}

void V23PowerModule::invokeMenuItem(const QString &id)
{
    if (id == "settings")
        showControlCenterModule("power");
}

V23NotificationModule::V23NotificationModule(QObject *parent)
    : V23QuickModule(parent)
    , m_notification(new V23DBusObject(QDBusConnection::sessionBus(), "com.deepin.dde.Notification",
                                       "/com/deepin/dde/Notification", "com.deepin.dde.Notification", this))
    , m_count(0)
{
    QDBusConnection::sessionBus().connect("com.deepin.dde.Notification", "/com/deepin/dde/Notification",
                                          "com.deepin.dde.Notification", "RecordAdded",
                                          this, SLOT(updateCount()));
    updateCount();
}

QString V23NotificationModule::displayName() const
{
    return tr("Notification Center");
}

V23Icon V23NotificationModule::dockIcon() const
{
    return V23Icon { QStringLiteral("notification"), QSize(16, 16) };
}

void V23NotificationModule::updateCount()
{
    m_notification->call("GetAllRecords", {}, [this](const QVariant &records) {
        m_count = QJsonDocument::fromJson(records.toString().toUtf8()).array().size();
        emit changed();
    });
}

void V23NotificationModule::aboutToShowTip()
{
    // the count may have dropped in the notification center meanwhile
    updateCount();
}

QString V23NotificationModule::tipText() const
{
    if (m_count == 0)
        return tr("No messages");
    return QString("%1 %2").arg(m_count).arg(tr("Notifications"));
}

QString V23NotificationModule::dockCommand() const
{
    return QStringLiteral("dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.ShowNotify");
}

V23QuickModule::MenuItems V23NotificationModule::menuItems() const
{
    return { { QStringLiteral("settings"), tr("Notification settings") } };
}

void V23NotificationModule::invokeMenuItem(const QString &id)
{
    if (id == "settings")
        showControlCenterModule("notification");
}

V23OnboardModule::V23OnboardModule(QObject *parent)
    : V23QuickModule(parent)
{
}

QString V23OnboardModule::displayName() const
{
    return tr("Onboard");
}

bool V23OnboardModule::isAvailable() const
{
    return QFile::exists("/usr/bin/onboard") || QFile::exists("/usr/bin/kylin-virtual-keyboard");
}

V23Icon V23OnboardModule::dockIcon() const
{
    return V23Icon { QStringLiteral("keyboard") };
}

QString V23OnboardModule::dockCommand() const
{
    // the same keyboards as the built-in onboard plugin
    if (QFile::exists("/usr/bin/kylin-virtual-keyboard"))
        return QString("dbus-send --print-reply --dest=org.fcitx.Fcitx5 /virtualkeyboard org.fcitx.Fcitx.VirtualKeyboard1.ToggleVirtualKeyboard");
    return QString("dbus-send --print-reply --dest=org.onboard.Onboard /org/onboard/Onboard/Keyboard org.onboard.Onboard.Keyboard.ToggleVisible");
}

QWidget *V23OnboardModule::createTile(QWidget *parent)
{
    V23QuickTile *tile = new V23QuickTile(V23QuickTile::Standard, parent);
    tile->setIcon(QStringLiteral("keyboard"));
    tile->setTitle(displayName());
    connect(tile, &V23QuickTile::clicked, this, [this] {
        emit requestHidePopup();
        V23Quick::runCommand(dockCommand());
    });
    return tile;
}

V23BrightnessModule::V23BrightnessModule(QObject *parent)
    : V23QuickModule(parent)
    , m_display(new V23DBusObject(QDBusConnection::sessionBus(), "com.deepin.daemon.Display",
                                  "/com/deepin/daemon/Display", "com.deepin.daemon.Display", this))
    , m_available(false)
{
    // on X11 the display daemon sets the brightness, while on Wayland only the
    // compositor can, like gxde-control-center does
    connect(m_display, &V23DBusObject::propertiesChanged, this, [this] {
        if (!usesWlcom())
            refresh();
    });

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(WlcomScreenService, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &V23BrightnessModule::refreshWlcom);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        m_wlcomBrightness.clear();
        refresh();
    });
    refreshWlcom();
}

bool V23BrightnessModule::usesWlcom() const
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(WlcomScreenService);
}

// the outputs of the compositor, with their brightness in percent
void V23BrightnessModule::refreshWlcom()
{
    if (!usesWlcom())
        return;

    QDBusMessage message = QDBusMessage::createMethodCall("com.kylin.Wlcom", "/com/kylin/Wlcom/Output",
                                                          "com.kylin.Wlcom.Output", "ListAllOutputs");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return;

        m_wlcomBrightness.clear();
        m_wlcomPrimary.clear();
        const QDBusArgument outputs = reply.arguments().first().value<QDBusArgument>();
        outputs.beginArray();
        while (!outputs.atEnd()) {
            QString name;
            QString json;
            outputs.beginStructure();
            outputs >> name >> json;
            outputs.endStructure();

            const QJsonObject output = QJsonDocument::fromJson(json.toUtf8()).object();
            if (!output.value("enabled").toBool(true))
                continue;
            m_wlcomBrightness.insert(name, output.value("brightness").toInt(100) / 100.0);
            if (output.value("primary").toBool() || m_wlcomPrimary.isEmpty())
                m_wlcomPrimary = name;
        }
        outputs.endArray();
        refresh();
    });
}

QMap<QString, double> V23BrightnessModule::brightnessMap() const
{
    return usesWlcom() ? m_wlcomBrightness : toMap<double>(m_display->property("Brightness"));
}

QString V23BrightnessModule::primary() const
{
    return usesWlcom() ? m_wlcomPrimary : m_display->property("Primary").toString();
}

void V23BrightnessModule::refresh()
{
    const bool available = brightnessMap().contains(primary());
    if (available != m_available) {
        m_available = available;
        emit availableChanged();
    }
    updateRow();
    updatePages();
}

QString V23BrightnessModule::displayName() const
{
    return tr("Brightness");
}

bool V23BrightnessModule::isAvailable() const
{
    return m_available;
}

double V23BrightnessModule::brightness() const
{
    return brightnessMap().value(primary());
}

void V23BrightnessModule::updateRow()
{
    if (m_row) {
        m_row->setValue(int(std::round(brightness() * 100)));
        m_row->setRightIcon(monitorIcon());
    }
}

QString V23BrightnessModule::monitorIcon() const
{
    // BrightnessQuickPanel: a laptop for the built-in screen
    const QString primary = this->primary();
    return primary.contains("eDP", Qt::CaseInsensitive) || primary.startsWith("LVDS", Qt::CaseInsensitive)
        ? QStringLiteral("laptop") : QStringLiteral("external-display");
}

void V23BrightnessModule::setBrightness(const QString &monitor, int value)
{
    if (monitor.isEmpty())
        return;

    if (!usesWlcom()) {
        m_display->call("SetAndSaveBrightness", {monitor, value / 100.0});
        return;
    }

    // the compositor tells no change, keep its value here
    QDBusMessage message = QDBusMessage::createMethodCall(WlcomScreenService, "/top/gxde/Wlcom/Screen",
                                                          "top.gxde.Wlcom.Screen", "SetScreenBrightness");
    message.setArguments({monitor, qBound(0, value, 100)});
    QDBusConnection::sessionBus().asyncCall(message);
    m_wlcomBrightness.insert(monitor, value / 100.0);
}

QWidget *V23BrightnessModule::createRow(QWidget *parent)
{
    m_row = new V23SliderRow(V23SliderRow::Panel, parent);
    m_row->setLeftIcon(QStringLiteral("brightness"));
    // the same range as dde-dock: 10% - 100%, so the screen never goes black
    m_row->setRange(10, 100);
    updateRow();

    connect(m_row, &V23SliderRow::valueChanged, this, [this](int value) {
        setBrightness(primary(), value);
    });
    // the compositor signals no change, read it again when the panel opens
    m_row->installEventFilter(this);
    connect(m_row, &V23SliderRow::rightClicked, this, &V23BrightnessModule::requestPage);
    connect(m_row, &V23SliderRow::clicked, this, &V23BrightnessModule::requestPage);

    return m_row;
}

bool V23BrightnessModule::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show)
        refreshWlcom();

    return V23QuickModule::eventFilter(watched, event);
}

QWidget *V23BrightnessModule::createPage(QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QVBoxLayout *monitors = new QVBoxLayout;
    monitors->setContentsMargins(0, 0, 0, 0);
    monitors->setSpacing(0);
    layout->addLayout(monitors);
    m_pages << Page { monitors, {} };
    page->installEventFilter(this);

    V23ListItem *settings = new V23ListItem(page);
    settings->setText(tr("Display settings"));
    settings->setArrow(true);
    layout->addWidget(settings);
    connect(settings, &V23ListItem::clicked, this, [this] {
        emit requestHidePopup();
        showControlCenterModule("display");
    });

    updatePages();
    return page;
}

// BrightnessApplet: a slider for each screen
void V23BrightnessModule::updatePages()
{
    const QMap<QString, double> brightness = brightnessMap();
    for (Page &page : m_pages) {
        if (!page.layout)
            continue;

        // only a change of the screens rebuilds the sliders, not one being dragged
        if (page.rows.keys() != brightness.keys()) {
            while (QLayoutItem *item = page.layout->takeAt(0)) {
                delete item->widget();
                delete item;
            }
            page.rows.clear();

            for (auto it = brightness.constBegin(); it != brightness.constEnd(); ++it) {
                const QString monitor = it.key();
                page.layout->addWidget(new V23SectionHeader(monitor, false));
                page.layout->addSpacing(5);

                V23SliderRow *row = new V23SliderRow(V23SliderRow::Page);
                row->setLeftIcon(QStringLiteral("brightness"));
                row->setRange(10, 100);
                page.layout->addWidget(row);
                page.layout->addSpacing(V23Quick::PanelSpacing);
                page.rows.insert(monitor, row);
                connect(row, &V23SliderRow::valueChanged, this, [this, monitor](int value) {
                    setBrightness(monitor, value);
                });
            }
        }

        for (auto it = brightness.constBegin(); it != brightness.constEnd(); ++it) {
            if (V23SliderRow *row = page.rows.value(it.key()))
                row->setValue(int(std::round(it.value() * 100)));
        }
    }
}
