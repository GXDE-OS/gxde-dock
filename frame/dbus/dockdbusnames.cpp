/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>

#include "dockdbusnames.h"

namespace {

const char* GXDE_SERVICE = "top.gxde.daemon.dock";
const char* GXDE_PATH = "/top/gxde/daemon/dock";
const char* GXDE_MANAGER = "top.gxde.daemon.dock";
const char* GXDE_ENTRY = "top.gxde.daemon.dock.Entry";

const char* DEEPIN_SERVICE = "com.deepin.dde.daemon.Dock";
const char* DEEPIN_PATH = "/com/deepin/dde/daemon/Dock";
const char* DEEPIN_MANAGER = "com.deepin.dde.daemon.Dock";
const char* DEEPIN_ENTRY = "com.deepin.dde.daemon.Dock.Entry";

bool detectGxdeDaemon() {
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    if (!bus) {
        return false;
    }

    const bool isGxdeDaemon = bus->isServiceRegistered(GXDE_SERVICE).value()
        || bus->activatableServiceNames().value().contains(GXDE_SERVICE);

    qInfo() << "(Dock) DBus: Reporting current bakend"
        << (isGxdeDaemon ? GXDE_SERVICE : DEEPIN_SERVICE);
    return isGxdeDaemon;
}

// Wraps detectGxdeDaemon() STATICALLY so that result is only queried ONCE,
// and same result is ALWAYS returned.
bool usingGxdeDaemon() {
    static const bool result = detectGxdeDaemon();
    return result;
}

}  // namespace

bool dockDBusDaemonAvailable() {
    if (usingGxdeDaemon()) {
        return true;
    }

    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(DEEPIN_SERVICE).value();
}

QString dockDBusService() {
    return QString::fromLatin1(usingGxdeDaemon() ? GXDE_SERVICE
        : DEEPIN_SERVICE);
}

QString dockDBusManagerPath() {
    return QString::fromLatin1(usingGxdeDaemon() ? GXDE_PATH : DEEPIN_PATH);
}

const char* dockDBusManagerInterface() {
    return usingGxdeDaemon() ? GXDE_MANAGER : DEEPIN_MANAGER;
}

const char* dockDBusEntryInterface() {
    return usingGxdeDaemon() ? GXDE_ENTRY : DEEPIN_ENTRY;
}
