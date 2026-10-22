// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.

#include "v23datetimeplugin.h"
#include "v23calendarwidget.h"
#include "v23datetimewidget.h"
#include "../../../widgets/tipswidget.h"

#include <DDBusSender>
#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>

// same keys as the built-in datetime plugin
#define PLUGIN_STATE_KEY "enable"
#define TIME_FORMAT_KEY "24HourFormat"

V23DatetimePlugin::V23DatetimePlugin(QObject *parent)
    : QObject(parent)
    , m_refreshTimer(nullptr)
{
}

const QString V23DatetimePlugin::pluginName() const
{
    return "datetime";
}

const QString V23DatetimePlugin::pluginDisplayName() const
{
    return tr("Datetime");
}

void V23DatetimePlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    if (pluginIsDisable())
        return;

    loadPlugin();
}

void V23DatetimePlugin::loadPlugin()
{
    if (m_centralWidget)
        return;

    m_dateTipsLabel = new TipsWidget;
    m_dateTipsLabel->setObjectName("datetime");

    m_centralWidget = new V23DatetimeWidget;
    m_centralWidget->set24HourFormat(m_proxyInter->getValue(this, TIME_FORMAT_KEY, true).toBool());

    m_calendarPopup = new V23CalendarWidget;

    connect(m_centralWidget, &V23DatetimeWidget::requestUpdateGeometry, this, [this] {
        m_proxyInter->itemUpdate(this, pluginName());
    });
    connect(m_calendarPopup, &V23CalendarWidget::jumpButtonClicked, this, [this] {
        m_proxyInter->requestSetAppletVisible(this, pluginName(), false);
        raiseCalendar();
    });

    // start on a whole second, so the minute flips together with the system clock
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &V23DatetimePlugin::updateCurrentTimeString);
    QTimer::singleShot(1000 - QTime::currentTime().msec(), this, [this] {
        updateCurrentTimeString();
        m_refreshTimer->start();
    });
    updateCurrentTimeString();

    m_proxyInter->itemAdded(this, pluginName());
}

void V23DatetimePlugin::pluginStateSwitched()
{
    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, pluginIsDisable());

    refreshPluginItemsVisible();
}

bool V23DatetimePlugin::pluginIsDisable()
{
    return !(m_proxyInter->getValue(this, PLUGIN_STATE_KEY, true).toBool());
}

int V23DatetimePlugin::itemSortKey(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    const QString key = QString("pos_%1").arg(displayMode());
    return m_proxyInter->getValue(this, key, -1).toInt();
}

void V23DatetimePlugin::setSortKey(const QString &itemKey, const int order)
{
    Q_UNUSED(itemKey);

    const QString key = QString("pos_%1").arg(displayMode());
    m_proxyInter->saveValue(this, key, order);
}

QWidget *V23DatetimePlugin::itemWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_centralWidget;
}

QWidget *V23DatetimePlugin::itemTipsWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_dateTipsLabel;
}

QWidget *V23DatetimePlugin::itemPopupApplet(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_calendarPopup;
}

const QString V23DatetimePlugin::itemContextMenu(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    QList<QVariant> items;

    QMap<QString, QVariant> settings;
    settings["itemId"] = "settings";
    if (m_centralWidget->is24HourFormat())
        settings["itemText"] = tr("12 Hour Time");
    else
        settings["itemText"] = tr("24 Hour Time");
    settings["isActive"] = true;
    items.push_back(settings);

    QMap<QString, QVariant> open;
    open["itemId"] = "open";
    open["itemText"] = tr("Time Settings");
    open["isActive"] = true;
    items.push_back(open);

    QMap<QString, QVariant> calendar;
    calendar["itemId"] = "calendar";
    calendar["itemText"] = tr("Open the calendar");
    calendar["isActive"] = true;
    items.push_back(calendar);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void V23DatetimePlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(itemKey)
    Q_UNUSED(checked)

    if (menuId == "open") {
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("ShowModule"))
            .arg(QString("datetime"))
            .call();
    } else if (menuId == "calendar") {
        raiseCalendar();
    } else if (menuId == "settings") {
        const bool value = m_proxyInter->getValue(this, TIME_FORMAT_KEY, true).toBool();
        m_proxyInter->saveValue(this, TIME_FORMAT_KEY, !value);
        m_centralWidget->set24HourFormat(!value);
        updateCurrentTimeString();
    }
}

void V23DatetimePlugin::positionChanged(const Dock::Position position)
{
    Q_UNUSED(position);

    if (m_centralWidget)
        m_centralWidget->updateDateTime();
}

void V23DatetimePlugin::pluginSettingsChanged()
{
    if (m_centralWidget)
        m_centralWidget->set24HourFormat(m_proxyInter->getValue(this, TIME_FORMAT_KEY, true).toBool());

    refreshPluginItemsVisible();
}

void V23DatetimePlugin::updateCurrentTimeString()
{
    const QDateTime current = QDateTime::currentDateTime();
    const QString date = current.date().toString(QLocale().dateFormat(QLocale::LongFormat));

    if (m_centralWidget->is24HourFormat())
        m_dateTipsLabel->setText(date + current.toString(" HH:mm:ss"));
    else
        m_dateTipsLabel->setText(date + current.toString(" hh:mm:ss AP"));

    m_centralWidget->updateDateTime();
}

void V23DatetimePlugin::refreshPluginItemsVisible()
{
    if (!pluginIsDisable()) {
        if (!m_centralWidget) {
            loadPlugin();
            return;
        }
        m_proxyInter->itemAdded(this, pluginName());
    } else {
        m_proxyInter->itemRemoved(this, pluginName());
    }
}

void V23DatetimePlugin::raiseCalendar()
{
    DDBusSender()
        .service("com.deepin.Calendar")
        .interface("com.deepin.Calendar")
        .path("/com/deepin/Calendar")
        .method(QString("RaiseWindow"))
        .call();
}
