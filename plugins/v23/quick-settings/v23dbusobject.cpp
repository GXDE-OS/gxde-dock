// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23dbusobject.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

static const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

V23DBusObject::V23DBusObject(const QDBusConnection &bus, const QString &service, const QString &path,
                             const QString &interface, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_service(service)
    , m_path(path)
    , m_interface(interface)
    , m_valid(false)
{
    m_bus.connect(m_service, m_path, PropertiesInterface, QStringLiteral("PropertiesChanged"),
                  this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(m_service, m_bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &V23DBusObject::refresh);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        m_valid = false;
        m_properties.clear();
        emit invalidated();
    });

    refresh();
}

QDBusPendingCall V23DBusObject::call(const QString &method, const QVariantList &args) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, m_path, m_interface, method);
    message.setArguments(args);
    return m_bus.asyncCall(message);
}

void V23DBusObject::call(const QString &method, const QVariantList &args, std::function<void(const QVariant &)> callback)
{
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call(method, args), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, callback] {
        const QDBusMessage reply = watcher->reply();
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            callback(reply.arguments().first());
        watcher->deleteLater();
    });
}

void V23DBusObject::setProperty(const QString &name, const QVariant &value)
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, m_path, PropertiesInterface, QStringLiteral("Set"));
    message.setArguments({m_interface, name, QVariant::fromValue(QDBusVariant(value))});
    m_bus.asyncCall(message);
}

void V23DBusObject::refresh()
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, m_path, PropertiesInterface, QStringLiteral("GetAll"));
    message.setArguments({m_interface});

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
            return;

        m_valid = true;
        m_properties = reply.value();
        emit propertiesChanged(m_properties.keys());
    });
}

void V23DBusObject::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != m_interface)
        return;

    for (auto it = changed.constBegin(); it != changed.constEnd(); ++it)
        m_properties.insert(it.key(), it.value());

    if (!invalidated.isEmpty()) {
        // rarely used by the deepin daemons, simply fetch everything again
        refresh();
    }

    if (!changed.isEmpty())
        emit propertiesChanged(changed.keys());
}
