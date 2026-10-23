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

#ifndef ABSTRACTPLUGINSCONTROLLER_H
#define ABSTRACTPLUGINSCONTROLLER_H

#include "pluginproxyinterface.h"
#include "pluginloader.h"

#include "dbus/dockdaemoninterface.h"

#include <QPluginLoader>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QDBusConnectionInterface>

// qApp property: the built-in plugins that the loaded V23 replacements take
// over in V23 mode, as paths relative to a built-in plugins dir, e.g.
// "libdatetime.so" or "system-trays/libsound.so"
#define PROP_V23_REPLACED_PLUGINS "V23ReplacedPlugins"

using DockDaemonInter = DockDaemonInterface;

class PluginsItemInterface;
class AbstractPluginsController : public QObject, PluginProxyInterface
{
    Q_OBJECT

public:
    explicit AbstractPluginsController(QObject *parent = 0);

    // implements PluginProxyInterface
    void saveValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant &value) Q_DECL_OVERRIDE;
    const QVariant getValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant& fallback = QVariant()) Q_DECL_OVERRIDE;
    void removeValue(PluginsItemInterface * const itemInter, const QStringList &keyList) override;

    // false for a plugin whose items are held back in the current display
    // mode: V23 replacements outside V23 mode, the built-ins they replace in it
    bool isPluginActive(PluginsItemInterface * const itemInter) const;

protected:
    QMap<PluginsItemInterface *, QMap<QString, QObject *>> &pluginsMap();
    QObject *pluginItemAt(PluginsItemInterface * const itemInter, const QString &itemKey) const;
    PluginsItemInterface *pluginInterAt(const QString &itemKey);
    PluginsItemInterface *pluginInterAt(QObject *destItem);
    // To be called first in itemAdded() and itemRemoved(). True when the item
    // belongs to an inactive plugin, it is then kept aside and the call must
    // go no further.
    bool holdInactiveItem(PluginsItemInterface * const itemInter, const QString &itemKey);
    bool releaseInactiveItem(PluginsItemInterface * const itemInter, const QString &itemKey);

protected Q_SLOTS:
    void startLoader(PluginLoader *loader);

private slots:
    void displayModeChanged();
    void positionChanged();
    void loadPlugin(const QString &pluginFile);
    void initPlugin(PluginsItemInterface *interface);
    void refreshPluginSettings();

private:
    bool eventFilter(QObject *o, QEvent *e) Q_DECL_OVERRIDE;
    bool isV23Replacement(PluginsItemInterface * const itemInter) const;
    QString builtinPluginPath(PluginsItemInterface * const itemInter) const;
    void publishV23Replacements(PluginsItemInterface * const itemInter, const QJsonObject &meta);
    void refreshActivePlugins();

private:
    QDBusConnectionInterface *m_dbusDaemonInterface;
    DockDaemonInter *m_dockDaemonInter;

    QMap<PluginsItemInterface *, QMap<QString, QObject *>> m_pluginsMap;
    QHash<PluginsItemInterface *, QString> m_pluginFiles;
    QSet<PluginsItemInterface *> m_inactivePlugins;
    QMap<PluginsItemInterface *, QStringList> m_inactiveItems;

    QJsonObject m_pluginSettingsObject;
};

#endif // ABSTRACTPLUGINSCONTROLLER_H
