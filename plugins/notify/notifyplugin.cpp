/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "notifyplugin.h"
#include "dbus/dbusaccount.h"

#include <QIcon>
#include <QSettings>

#define PLUGIN_STATE_KEY    "enable"

NotifyPlugin::NotifyPlugin(QObject *parent)
    : QObject(parent),

      m_pluginLoaded(false),
      m_tipsLabel(new TipsWidget)
{
    m_tipsLabel->setVisible(false);

    m_isInChroot = false;
}

const QString NotifyPlugin::pluginName() const
{
    return "notify";
}

const QString NotifyPlugin::pluginDisplayName() const
{
    return tr("Power");
}

QWidget *NotifyPlugin::itemWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_notifyWidget;
}

QWidget *NotifyPlugin::itemTipsWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    // reset text every time to avoid size of LabelWidget not change after
    // font size be changed in ControlCenter
    m_tipsLabel->setText(tr("Notify"));

    return m_tipsLabel;
}

void NotifyPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    // transfer config
    QSettings settings("deepin", "dde-dock-notify");
    if (QFile::exists(settings.fileName())) {
        QFile::remove(settings.fileName());
    }

    if (!pluginIsDisable()) {
        loadPlugin();
    }
}

void NotifyPlugin::pluginStateSwitched()
{
    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, !m_proxyInter->getValue(this, PLUGIN_STATE_KEY, true).toBool());

    refreshPluginItemsVisible();
}

bool NotifyPlugin::pluginIsDisable()
{
    if (m_isInChroot) {
        return 1;
    }
    return !m_proxyInter->getValue(this, PLUGIN_STATE_KEY, true).toBool();
}

const QString NotifyPlugin::itemCommand(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return QString("dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.ShowNotify");
}

const QString NotifyPlugin::itemContextMenu(const QString &itemKey)
{
    QList<QVariant> items;
    items.reserve(1);

    QMap<QString, QVariant> controlCenter;
    controlCenter["itemId"] = "controlCenter";
    controlCenter["itemText"] = tr("Open Control Center");
    controlCenter["isActive"] = true;
    items.push_back(controlCenter);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void NotifyPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(itemKey)
    Q_UNUSED(checked)

    if (menuId == "controlCenter") {
        QProcess::startDetached("dbus-send --session --print-reply=literal --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.Show");
    }
}

void NotifyPlugin::displayModeChanged(const Dock::DisplayMode displayMode)
{
    Q_UNUSED(displayMode);

    if (!pluginIsDisable()) {
        m_notifyWidget->update();
    }
}

int NotifyPlugin::itemSortKey(const QString &itemKey)
{
    Dock::DisplayMode mode = displayMode();
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(mode);

    if (mode == Dock::DisplayMode::Fashion) {
        return m_proxyInter->getValue(this, key, 2).toInt();
    } else {
        return m_proxyInter->getValue(this, key, 5).toInt();
    }
}

void NotifyPlugin::setSortKey(const QString &itemKey, const int order)
{
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(displayMode());
    m_proxyInter->saveValue(this, key, order);
}

void NotifyPlugin::pluginSettingsChanged()
{
    refreshPluginItemsVisible();
}

void NotifyPlugin::loadPlugin()
{
    if (m_pluginLoaded) {
        qDebug() << "notify plugin has been loaded! return";
        return;
    }

    m_pluginLoaded = true;

    m_notifyWidget = new PluginWidget;

    m_proxyInter->itemAdded(this, pluginName());
    displayModeChanged(displayMode());
}

void NotifyPlugin::refreshPluginItemsVisible()
{
    if (pluginIsDisable())
    {
        m_proxyInter->itemRemoved(this, pluginName());
    } else {
        if (!m_pluginLoaded) {
            loadPlugin();
            return;
        }
        m_proxyInter->itemAdded(this, pluginName());
    }
}
