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

#include "abstractpluginscontroller.h"
#include "pluginsiteminterface.h"
#include "DNotifySender"

#include "waylandhelper.h"
#include "dbus/dockdbusnames.h"
#include <QDebug>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGSettings>
#include <QJsonArray>
#include <QSet>
#include <QTemporaryFile>

static const QStringList CompatiblePluginApiList {
    "1.1.1",
    "1.2",
    DOCK_PLUGIN_API_VERSION
};

AbstractPluginsController::AbstractPluginsController(QObject *parent)
    : QObject(parent)
    , m_dbusDaemonInterface(QDBusConnection::sessionBus().interface())
    , m_dockDaemonInter(new DockDaemonInter(dockDBusService(), dockDBusManagerPath(), QDBusConnection::sessionBus(), this))
{
    qApp->installEventFilter(this);

    refreshPluginSettings();

    connect(m_dockDaemonInter, &DockDaemonInter::PluginSettingsSynced, this, &AbstractPluginsController::refreshPluginSettings, Qt::QueuedConnection);
}

void AbstractPluginsController::saveValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant &value) {
    // is it necessary?
//    refreshPluginSettings();

    // save to local cache
    QJsonObject localObject = m_pluginSettingsObject.value(itemInter->pluginName()).toObject();
    const QJsonValue jsonValue = QJsonValue::fromVariant(value);
    if (localObject.contains(key) && localObject.value(key) == jsonValue) {
        return;
    }
    localObject.insert(key, jsonValue); //Note: QVariant::toJsonValue() not work in Qt 5.7
    m_pluginSettingsObject.insert(itemInter->pluginName(), localObject);

    // save to daemon
    QJsonObject remoteObject, remoteObjectInter;
    remoteObjectInter.insert(key, jsonValue); //Note: QVariant::toJsonValue() not work in Qt 5.7
    remoteObject.insert(itemInter->pluginName(), remoteObjectInter);
    m_dockDaemonInter->MergePluginSettings(QJsonDocument(remoteObject).toJson(QJsonDocument::JsonFormat::Compact));
}

const QVariant AbstractPluginsController::getValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant& fallback) {
    // load from local cache
    QVariant v = m_pluginSettingsObject.value(itemInter->pluginName()).toObject().value(key).toVariant();
    if (v.isNull() || !v.isValid()) {
        v = fallback;
    }

    return v;
}

void AbstractPluginsController::removeValue(PluginsItemInterface * const itemInter, const QStringList &keyList)
{
    if (keyList.isEmpty()) {
        m_pluginSettingsObject.remove(itemInter->pluginName());
    } else {
        QJsonObject localObject = m_pluginSettingsObject.value(itemInter->pluginName()).toObject();
        for (auto key : keyList) {
            localObject.remove(key);
        }
        m_pluginSettingsObject.insert(itemInter->pluginName(), localObject);
    }

    m_dockDaemonInter->RemovePluginSettings(itemInter->pluginName(), keyList);
}

QMap<PluginsItemInterface *, QMap<QString, QObject *> > &AbstractPluginsController::pluginsMap()
{
    return m_pluginsMap;
}

QObject *AbstractPluginsController::pluginItemAt(PluginsItemInterface * const itemInter, const QString &itemKey) const
{
    if (!m_pluginsMap.contains(itemInter))
        return nullptr;

    return m_pluginsMap[itemInter][itemKey];
}

PluginsItemInterface *AbstractPluginsController::pluginInterAt(const QString &itemKey)
{
    for (auto it = m_pluginsMap.constBegin(); it != m_pluginsMap.constEnd(); ++it) {
        for (auto key : it.value().keys()) {
            if (key == itemKey) {
                return it.key();
            }
        }
    }

    return nullptr;
}

PluginsItemInterface *AbstractPluginsController::pluginInterAt(QObject *destItem)
{
    for (auto it = m_pluginsMap.constBegin(); it != m_pluginsMap.constEnd(); ++it) {
        for (auto item : it.value().values()) {
            if (item == destItem) {
                return it.key();
            }
        }
    }

    return nullptr;
}

// Where the built-in plugins live: next to a dock run from its build tree,
// and the installed ones
static const QStringList &builtinPluginsDirs()
{
    static const QStringList dirs = [] {
        QStringList list;
        for (const QString &path : {QCoreApplication::applicationDirPath() + "/../plugins",
                                    QStringLiteral("/usr/lib/gxde-dock/plugins")}) {
            const QString canonical = QDir(path).canonicalPath();
            if (!canonical.isEmpty() && !list.contains(canonical))
                list << canonical;
        }
        return list;
    }();

    return dirs;
}

bool AbstractPluginsController::isPluginActive(PluginsItemInterface * const itemInter) const
{
    if (isV23Replacement(itemInter))
        return Dock::isV23Mode();

    if (!Dock::isV23Mode())
        return true;
    static const QStringList v23Dropped { "libddegrandsearch_dockplugin.so" };

    // Only the very built-in file is replaced, never a plugin the user installed
    const QString builtin = builtinPluginPath(itemInter);
    return builtin.isEmpty()
        || !(v23Dropped.contains(builtin)
             || qApp->property(PROP_V23_REPLACED_PLUGINS).toStringList().contains(builtin));
}

QObject *AbstractPluginsController::pluginObject(PluginsItemInterface * const itemInter) const {
    return m_pluginObjects.value(itemInter);
}

bool AbstractPluginsController::holdInactiveItem(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    if (!m_inactivePlugins.contains(itemInter) && isPluginActive(itemInter))
        return false;

    QStringList &keys = m_inactiveItems[itemInter];
    if (!keys.contains(itemKey))
        keys << itemKey;

    // Items it already shows are taken down by refreshActivePlugins()
    if (m_pluginsMap.value(itemInter).isEmpty())
        m_inactivePlugins.insert(itemInter);

    return true;
}

bool AbstractPluginsController::releaseInactiveItem(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    auto it = m_inactiveItems.find(itemInter);
    return it != m_inactiveItems.end() && it->removeOne(itemKey);
}

bool AbstractPluginsController::isV23Replacement(PluginsItemInterface * const itemInter) const
{
    const QString dir = QFileInfo(m_pluginFiles.value(itemInter)).path();
    for (const QString &builtinDir : builtinPluginsDirs()) {
        if (dir == builtinDir + "/v23")
            return true;
    }

    return false;
}

QString AbstractPluginsController::builtinPluginPath(PluginsItemInterface * const itemInter) const
{
    const QString file = m_pluginFiles.value(itemInter);
    for (const QString &builtinDir : builtinPluginsDirs()) {
        if (file.startsWith(builtinDir + "/"))
            return file.mid(builtinDir.size() + 1);
    }

    return QString();
}

// A V23 replacement takes over the built-in of the same file name, and those
// its metadata lists in "replaces"
void AbstractPluginsController::publishV23Replacements(PluginsItemInterface * const itemInter, const QJsonObject &meta)
{
    if (!isV23Replacement(itemInter))
        return;

    QStringList replaced = qApp->property(PROP_V23_REPLACED_PLUGINS).toStringList();
    QStringList entries { QFileInfo(m_pluginFiles.value(itemInter)).fileName() };
    for (const QJsonValue &value : meta.value("replaces").toArray())
        entries << QDir::cleanPath(value.toString());

    bool changed = false;
    for (const QString &entry : entries) {
        if (!entry.isEmpty() && !replaced.contains(entry)) {
            replaced << entry;
            changed = true;
        }
    }

    // Every controller of the dock, including the system tray ones, listens
    if (changed)
        qApp->setProperty(PROP_V23_REPLACED_PLUGINS, replaced);
}

// Shows or holds back the items of the plugins whose activeness changed
void AbstractPluginsController::refreshActivePlugins()
{
    QList<PluginsItemInterface *> activated;
    const auto inters = m_pluginsMap.keys();

    for (PluginsItemInterface *inter : inters) {
        const bool active = isPluginActive(inter);
        if (!active && !m_inactivePlugins.contains(inter)) {
            const QStringList keys = m_pluginsMap.value(inter).keys();
            for (const QString &key : keys)
                itemRemoved(inter, key);

            QStringList &held = m_inactiveItems[inter];
            for (const QString &key : keys) {
                if (!held.contains(key))
                    held << key;
            }
            m_inactivePlugins.insert(inter);
        } else if (active && (m_inactivePlugins.remove(inter) || m_inactiveItems.contains(inter))) {
            for (const QString &key : m_inactiveItems.take(inter))
                itemAdded(inter, key);
            activated << inter;
        }
    }

    // A V15 plugin and its V23 replacement share their settings, which may
    // have changed in the meantime
    for (PluginsItemInterface *inter : activated)
        inter->pluginSettingsChanged();
}

void AbstractPluginsController::startLoader(PluginLoader *loader)
{
    connect(loader, &PluginLoader::finished, loader, &PluginLoader::deleteLater, Qt::QueuedConnection);
    connect(loader, &PluginLoader::pluginFounded, this, &AbstractPluginsController::loadPlugin, Qt::QueuedConnection);

    QGSettings gsetting("com.deepin.dde.dock", "/com/deepin/dde/dock/");

    QTimer::singleShot(gsetting.get("delay-plugins-time").toUInt(),
                       loader, [=] { loader->start(QThread::LowestPriority); });
}

void AbstractPluginsController::displayModeChanged()
{
    const Dock::DisplayMode displayMode = qApp->property(PROP_DISPLAY_MODE).value<Dock::DisplayMode>();
    const auto inters = m_pluginsMap.keys();

    for (auto inter : inters)
        inter->displayModeChanged(displayMode);

    refreshActivePlugins();
}

void AbstractPluginsController::positionChanged()
{
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    const auto inters = m_pluginsMap.keys();

    for (auto inter : inters)
        inter->positionChanged(position);
}

void AbstractPluginsController::loadPlugin(const QString &pluginFile)
{
    // Keep the module loaded for the application lifetime. Factory-created
    // plugin objects belong to a screen controller and may outlive this call.
    QPluginLoader *pluginLoader = new QPluginLoader(pluginFile, qApp);
    const QJsonObject &meta = pluginLoader->metaData().value("MetaData").toObject();
    const QString &pluginApi = meta.value("api").toString();
    if (pluginApi.isEmpty() || !CompatiblePluginApiList.contains(pluginApi))
    {
        qWarning() << objectName()
                   << "plugin api version not matched! expect versions:" << CompatiblePluginApiList
                   << ", got version:" << pluginApi
                   << ", the plugin file is:" << pluginFile;

        QString notifyMessage(tr("The incompatible plugin %1 was skipped."));
        Dtk::Core::DUtil::DNotifySender(notifyMessage.arg(QFileInfo(pluginFile).fileName())).appIcon("dialog-warning").call();
        pluginLoader->deleteLater();
        return;
    }

    QObject *rootObject = pluginLoader->instance();
    PluginsItemInterface *interface = nullptr;
    QObject *pluginObject = nullptr;
    if (auto *factory = qobject_cast<PluginsItemFactoryInterface *>(rootObject)) {
        QObject *instanceObject = factory->createPluginInstance();
        interface = qobject_cast<PluginsItemInterface *>(instanceObject);
        if (interface) {
            pluginObject = instanceObject;
            instanceObject->setParent(this);
        } else if (instanceObject) {
            delete instanceObject;
        }
    } else {
        // QPluginLoader shares one root QObject for the same library path. Old
        // plugins have no factory, so load an isolated on-disk copy per screen;
        // otherwise their single QWidget tree is reparented between two Docks.
        QObject *isolatedRoot = nullptr;
        auto *isolatedFile = new QTemporaryFile(
            QDir::tempPath() + QStringLiteral("/gxde-dock-plugin-XXXXXX.so"),
            qApp);
        QFile sourceFile(pluginFile);
        if (sourceFile.open(QIODevice::ReadOnly) && isolatedFile->open()) {
            bool copied = true;
            while (!sourceFile.atEnd()) {
                const QByteArray chunk = sourceFile.read(1024 * 1024);
                if (chunk.isEmpty() || isolatedFile->write(chunk) != chunk.size()) {
                    copied = false;
                    break;
                }
            }
            copied = copied && isolatedFile->flush();
            isolatedFile->close();

            if (copied) {
                auto *isolatedLoader = new QPluginLoader(isolatedFile->fileName(), qApp);
                isolatedRoot = isolatedLoader->instance();
                interface = qobject_cast<PluginsItemInterface *>(isolatedRoot);
                if (!interface) {
                    qWarning() << objectName()
                               << "failed to load isolated legacy plugin:"
                               << pluginFile << isolatedLoader->errorString();
                    isolatedLoader->deleteLater();
                } else {
                    pluginObject = isolatedRoot;
                    qInfo() << objectName() << "isolated legacy plugin instance:"
                            << pluginFile << isolatedRoot;
                }
            }
        }

        if (!interface) {
            isolatedFile->deleteLater();
        }

        // Copying may fail for a plugin with unusual filesystem constraints or
        // $ORIGIN-only dependencies. In that case retain the safe first-screen
        // fallback instead of stealing its widget into another Dock.
        static QSet<QString> initializedLegacyPlugins;
        const QString canonicalFile = QFileInfo(pluginFile).canonicalFilePath();
        if (!interface) {
            interface = qobject_cast<PluginsItemInterface *>(rootObject);
            if (!interface) {
                // The common error path below will report the loader error.
            } else if (initializedLegacyPlugins.contains(canonicalFile)) {
                qWarning() << objectName()
                           << "legacy plugin isolation unavailable; skip duplicate load:"
                           << pluginFile;
                pluginLoader->deleteLater();
                return;
            } else {
                pluginObject = rootObject;
                initializedLegacyPlugins.insert(canonicalFile);
            }
        }
    }

    if (!interface)
    {
        qWarning() << objectName() << "load plugin failed!!!" << pluginLoader->errorString() << pluginFile;

        QString notifyMessage(tr("The plugin %1 failed to load and was skipped."));
        Dtk::Core::DUtil::DNotifySender(notifyMessage.arg(QFileInfo(pluginFile).fileName())).appIcon("dialog-warning").call();
        pluginLoader->unload();
        pluginLoader->deleteLater();
        return;
    }

    // Record the file before init(), it decides if the plugin is active
    m_pluginFiles.insert(interface, QFileInfo(pluginFile).canonicalFilePath());
    m_pluginObjects.insert(interface, pluginObject);
    publishV23Replacements(interface, meta);

    QString dbusService = meta.value("depends-daemon-dbus-service").toString();
    const bool skipMissingService =
        Wayland::isWaylandSession()
        && dbusService == QStringLiteral("com.deepin.dde.TrayManager");
    if (!dbusService.isEmpty()
            && !skipMissingService
            && !m_dbusDaemonInterface->isServiceRegistered(dbusService).value()) {
        qDebug() << objectName() << dbusService << "daemon has not started, waiting for signal";
        
        // Use watcher to wait the signal for every plugin
        // So we coule wait async for each one.
        auto* watcher = new QDBusServiceWatcher(
            dbusService,
            QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration,
            this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, this,
            [=](const QString &) {
                qDebug() << objectName() << dbusService << "daemon started, init plugin";
                watcher->deleteLater();
                initPlugin(interface);
            });
        return;
    }

    // NOTE(justforlxz): 插件的所有初始化工作都在init函数中进行，
    // loadPlugin函数是按队列执行的，initPlugin函数会有可能导致
    // 函数执行被阻塞。
    QTimer::singleShot(1, this, [=] {
        initPlugin(interface);
    });
}

void AbstractPluginsController::initPlugin(PluginsItemInterface *interface) {
    // Do not expose a plugin through pluginsMap() before init().  Consumers
    // such as the settings menu may call methods which require m_proxyInter,
    // while plugins waiting for a D-Bus dependency are still uninitialized.
    m_pluginsMap.insert(interface, QMap<QString, QObject *>());
    qDebug() << objectName() << "init plugin: " << interface->pluginName();
    interface->init(this);
    qDebug() << objectName() << "init plugin finished: " << interface->pluginName();
}

void AbstractPluginsController::refreshPluginSettings()
{
    const QString &pluginSettings = m_dockDaemonInter->GetPluginSettings().value();
    if (pluginSettings.isEmpty()) {
        qDebug() << "Error! get plugin settings from dbus failed!";
        return;
    }

    const QJsonObject &pluginSettingsObject = QJsonDocument::fromJson(pluginSettings.toLocal8Bit()).object();
    if (pluginSettingsObject.isEmpty()) {
        return;
    }

    // nothing changed
    if (pluginSettingsObject == m_pluginSettingsObject) {
        return;
    }

    for (auto pluginsIt = pluginSettingsObject.constBegin(); pluginsIt != pluginSettingsObject.constEnd(); ++pluginsIt) {
        const QString &pluginName = pluginsIt.key();
        const QJsonObject &settingsObject = pluginsIt.value().toObject();
        QJsonObject newSettingsObject = m_pluginSettingsObject.value(pluginName).toObject();
        for (auto settingsIt = settingsObject.constBegin(); settingsIt != settingsObject.constEnd(); ++settingsIt) {
            newSettingsObject.insert(settingsIt.key(), settingsIt.value());
        }
        // TODO: remove not exists key-values
        m_pluginSettingsObject.insert(pluginName, newSettingsObject);
    }

    // not notify plugins to refresh settings if this update is not emit by dock daemon
    if (sender() != m_dockDaemonInter) {
        return;
    }

    // notify all plugins to reload plugin settings
    for (PluginsItemInterface *pluginInter : m_pluginsMap.keys()) {
        pluginInter->pluginSettingsChanged();
    }

}

bool AbstractPluginsController::eventFilter(QObject *o, QEvent *e)
{
    if (o != qApp)
        return false;
    if (e->type() != QEvent::DynamicPropertyChange)
        return false;

    QDynamicPropertyChangeEvent * const dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
    const QString propertyName = dpce->propertyName();

    if (propertyName == PROP_POSITION)
        positionChanged();
    else if (propertyName == PROP_DISPLAY_MODE)
        displayModeChanged();
    else if (propertyName == PROP_V23_REPLACED_PLUGINS)
        refreshActivePlugins();

    return false;
}
