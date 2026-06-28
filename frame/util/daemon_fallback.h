/*
 * Copyright (C) 2026 CharOfString <markus_verify@126.com>
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

#ifndef GXDE_DOCK_DAEMON_FALLBACK_H
#define GXDE_DOCK_DAEMON_FALLBACK_H

#include <QString>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <QStringLiteral>

namespace GXDEDockFallback {

inline const QString ORIGINAL_DOCK_SERVICE = QStringLiteral("com.deepin.dde.daemon.Dock");
inline const QString FALLBACK_DOCK_SERVICE = QStringLiteral("top.gxde.dock.fallback.service.Main");
inline const QString ORIGINAL_DISPLAY_SERVICE = QStringLiteral("com.deepin.daemon.Display");
inline const QString FALLBACK_DISPLAY_SERVICE = QStringLiteral("top.gxde.dock.fallback.service.Display");

// Detect if current session is under Wayland
inline bool isWayland() {
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        return true;
    }
    const QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    return sessionType == QStringLiteral("wayland")
        || sessionType == QStringLiteral("dwayland");
}

// Start fallback daemon
inline void launchFallbackDaemon() {
    auto* target_interface = QDBusConnection::sessionBus().interface();

    // If running, just do No ops...
    if (target_interface && target_interface->isServiceRegistered(
            FALLBACK_DOCK_SERVICE).value()) {
        qInfo() << "(Fallback service) already started:"
            << FALLBACK_DOCK_SERVICE;
        return;
    }

    qInfo() << "(Fallback service) Init: Launching fallback daemon...";
    const QString cwd = QStringLiteral(
        "/usr/libexec/gxde-dock/fallback-daemon");

    QProcess::startDetached(QStringLiteral("python3"),
        {QStringLiteral("-m"), QStringLiteral("dock")}, cwd);

    // Wait for D-Bus service to be registered
    for (int i = 0; i < 50; ++i) {
        QThread::msleep(100);
        QCoreApplication::processEvents();
        if (target_interface->isServiceRegistered(FALLBACK_DOCK_SERVICE)
                .value()) {
            qInfo() << "(Fallback service) Daemon shall be ready:"
                << FALLBACK_DOCK_SERVICE;
            return;
        }
    }
    qWarning() << "(Fallback service) Err: daemon startup timedout.";
}

/// If wayland and dde-daemon unavaliable, pull fallback service. (DOCK)
inline QString dockServiceName() {
    auto* target_interface = QDBusConnection::sessionBus().interface();

    // If original dde-daemon is available, use it.
    if (target_interface && target_interface->isServiceRegistered(
            ORIGINAL_DOCK_SERVICE).value()) {
        return ORIGINAL_DOCK_SERVICE;
    }

    // Launch fallback daemon under Wayland.
    if (isWayland()) {
        launchFallbackDaemon();
    }

    return FALLBACK_DOCK_SERVICE;
}

/// If wayland and dde-daemon unavaliable, pull fallback service. (DISPLAY)
inline QString displayServiceName() {
    auto* target_interface = QDBusConnection::sessionBus().interface();
    if (target_interface && target_interface->isServiceRegistered(
            ORIGINAL_DISPLAY_SERVICE).value()) {
        return ORIGINAL_DISPLAY_SERVICE;
    }

    if (isWayland()) {
        launchFallbackDaemon();
    }

    return FALLBACK_DISPLAY_SERVICE;
}

}  // namespace GXDEDockFallback

#endif // GXDE_DOCK_DAEMON_FALLBACK_H
