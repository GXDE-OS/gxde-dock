// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23DBUSOBJECT_H
#define V23DBUSOBJECT_H

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QObject>
#include <QVariantMap>

class QDBusPendingCallWatcher;

// Mirrors the properties of a D-Bus object, kept up to date with its
// PropertiesChanged signal, and calls its methods asynchronously
class V23DBusObject : public QObject
{
    Q_OBJECT

public:
    V23DBusObject(const QDBusConnection &bus, const QString &service, const QString &path,
                  const QString &interface, QObject *parent = nullptr);

    QString path() const { return m_path; }
    bool isValid() const { return m_valid; }
    QVariant property(const QString &name) const { return m_properties.value(name); }

    QDBusPendingCall call(const QString &method, const QVariantList &args = QVariantList()) const;
    // calls a method, and hands its first result to the callback
    void call(const QString &method, const QVariantList &args, std::function<void(const QVariant &)> callback);
    void setProperty(const QString &name, const QVariant &value);

    void refresh();

signals:
    void propertiesChanged(const QStringList &names) const;
    // the service went away
    void invalidated() const;

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    QDBusConnection m_bus;
    QString m_service;
    QString m_path;
    QString m_interface;
    QVariantMap m_properties;
    bool m_valid;
};

#endif // V23DBUSOBJECT_H
