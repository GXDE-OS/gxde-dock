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

#include "dockpluginscontroller.h"
#include "pluginsiteminterface.h"
#include "item/traypluginitem.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

DockPluginsController::DockPluginsController(QObject *parent)
    : AbstractPluginsController(parent)
{
    setObjectName("DockPlugin");
}

void DockPluginsController::itemAdded(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    if (!isPluginActive(itemInter)) {
        QStringList &keys = m_inactiveItems[itemInter];
        if (!keys.contains(itemKey)) {
            keys << itemKey;
        }

        return;
    }

    QMap<PluginsItemInterface *, QMap<QString, QObject *>> &mPluginsMap = pluginsMap();

    // check if same item added
    if (mPluginsMap.contains(itemInter))
        if (mPluginsMap[itemInter].contains(itemKey))
            return;

    PluginsItem *item = nullptr;
    if (itemInter->pluginName() == "tray") {
        item = new TrayPluginItem(itemInter, itemKey);
        connect(static_cast<TrayPluginItem *>(item), &TrayPluginItem::fashionTraySizeChanged,
                this, &DockPluginsController::fashionTraySizeChanged, Qt::UniqueConnection);
    } else {
        item = new PluginsItem(itemInter, itemKey);
    }

    item->setVisible(false);

    mPluginsMap[itemInter][itemKey] = item;

    emit pluginItemInserted(item);
}

void DockPluginsController::itemUpdate(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    PluginsItem *item = static_cast<PluginsItem *>(pluginItemAt(itemInter, itemKey));
    if (!item)
        return;

    item->update();

    emit pluginItemUpdated(item);
}

void DockPluginsController::itemRemoved(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    if (m_inactiveItems.contains(itemInter) && m_inactiveItems[itemInter].removeOne(itemKey)) {
        return;
    }

    PluginsItem *item = static_cast<PluginsItem *>(pluginItemAt(itemInter, itemKey));
    if (!item)
        return;

    item->detachPluginWidget();

    emit pluginItemRemoved(item);

    QMap<PluginsItemInterface *, QMap<QString, QObject *>> &mPluginsMap = pluginsMap();
    mPluginsMap[itemInter].remove(itemKey);

    // do not delete the itemWidget object(specified in the plugin interface)
    item->centralWidget()->setParent(nullptr);

    // just delete our wrapper object(PluginsItem)
    item->deleteLater();
}

void DockPluginsController::requestWindowAutoHide(PluginsItemInterface * const itemInter, const QString &itemKey, const bool autoHide)
{
    PluginsItem *item = static_cast<PluginsItem *>(pluginItemAt(itemInter, itemKey));
    if (!item)
        return;

    Q_EMIT item->requestWindowAutoHide(autoHide);
}

void DockPluginsController::requestRefreshWindowVisible(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    PluginsItem *item = static_cast<PluginsItem *>(pluginItemAt(itemInter, itemKey));
    if (!item)
        return;

    Q_EMIT item->requestRefreshWindowVisible();
}

void DockPluginsController::requestSetAppletVisible(PluginsItemInterface * const itemInter, const QString &itemKey, const bool visible)
{
    PluginsItem *item = static_cast<PluginsItem *>(pluginItemAt(itemInter, itemKey));
    if (!item)
        return;

    if (visible) {
        item->showPopupApplet(itemInter->itemPopupApplet(itemKey));
    } else {
        item->hidePopup();
    }
}

void DockPluginsController::startLoader()
{
    m_v23Mode = Dock::isV23Mode();

    loadLocalPlugins();
    loadSystemPlugins();
}

bool DockPluginsController::isPluginActive(PluginsItemInterface * const itemInter) const
{
    if (isV23Replacement(itemInter)) {
        return m_v23Mode;
    }

    return !(m_v23Mode && isReplacedByV23(itemInter));
}

bool DockPluginsController::isV23Replacement(PluginsItemInterface * const itemInter) const
{
    return !m_v23PluginsDir.isEmpty()
        && QFileInfo(pluginFile(itemInter)).canonicalPath() == m_v23PluginsDir;
}

bool DockPluginsController::isReplacedByV23(PluginsItemInterface * const itemInter) const
{
    // Only built in library that is named after V23 is replaced.
    // Installed ones will not be, for safety.
    const QFileInfo file(pluginFile(itemInter));
    if (m_systemPluginsDir.isEmpty() || file.canonicalPath() != m_systemPluginsDir) {
        return false;
    }

    for (const QString &loaded : pluginFiles()) {
        const QFileInfo replacement(loaded);
        if (replacement.fileName() == file.fileName()
                && replacement.canonicalPath() == m_v23PluginsDir) {
            return true;
        }
    }

    return false;
}

void DockPluginsController::afterDisplayModeChanged()
{
    const bool v23Mode = Dock::isV23Mode();
    if (v23Mode == m_v23Mode) {
        return;
    }

    QList<PluginsItemInterface *> wasActive;
    for (PluginsItemInterface *inter : pluginsMap().keys()) {
        if (isPluginActive(inter)) {
            wasActive << inter;
        }
    }

    m_v23Mode = v23Mode;

    QList<PluginsItemInterface *> activated;
    for (PluginsItemInterface *inter : pluginsMap().keys()) {
        const bool active = isPluginActive(inter);
        if (!active && wasActive.contains(inter)) {
            for (const QString &key : pluginsMap()[inter].keys()) {
                itemRemoved(inter, key);
                m_inactiveItems[inter] << key;
            }
        } else if (active && !wasActive.contains(inter)) {
            for (const QString &key : m_inactiveItems.take(inter))
                itemAdded(inter, key);
            activated << inter;
        }
    }

    // ALl of V15 & V23 shared settings!
    for (PluginsItemInterface *inter : activated) {
        inter->pluginSettingsChanged();
    }
}

void DockPluginsController::loadLocalPlugins() {
    QString pluginsDir(QString("%1/.local/lib/gxde-dock/plugins/").arg(QDir::homePath()));

    if (!QDir(pluginsDir).exists()) {
        return;
    }

    qDebug() << "using dock local plugins dir:" << pluginsDir;

    AbstractPluginsController::startLoader(new PluginLoader(pluginsDir, this));
}

void DockPluginsController::loadSystemPlugins() {
    QString pluginsDir(qApp->applicationDirPath() + "/../plugins");
    if (!QDir(pluginsDir).exists()) {
        pluginsDir = "/usr/lib/gxde-dock/plugins";
    }
    qDebug() << "using dock plugins dir:" << pluginsDir;

    m_systemPluginsDir = QDir(pluginsDir).canonicalPath();

    PluginLoader *systemLoader = new PluginLoader(pluginsDir, this);
    const QDir v23Dir(pluginsDir + "/v23");
    if (!v23Dir.exists()) {
        AbstractPluginsController::startLoader(systemLoader);
        return;
    }

    // Load the V23 replacements first, that inits them.
    m_v23PluginsDir = v23Dir.canonicalPath();
    qDebug() << "using dock V23 plugins dir:" << m_v23PluginsDir;

    PluginLoader *v23Loader = new PluginLoader(m_v23PluginsDir, this);
    connect(v23Loader, &PluginLoader::finished, this, [this, systemLoader] {
        AbstractPluginsController::startLoader(systemLoader);
    }, Qt::QueuedConnection);
    AbstractPluginsController::startLoader(v23Loader);
}
