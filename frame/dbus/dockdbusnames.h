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
 * ----------------------------------------------------------------------------
 * Simple auto selecter for D-Bus names. Prefers top.gxde.daemon.dock, which is
 * from GXDE-Daemon; and falls back to com.deepin.dde.daemon.Dock, which is
 * from Deepin-Daemon.
 */

#ifndef FRAME_DBUS_DOCKDBUSNAMES_H_
#define FRAME_DBUS_DOCKDBUSNAMES_H_

#include <QString>

bool dockDBusDaemonAvailable();

QString dockDBusService();
QString dockDBusManagerPath();
const char *dockDBusManagerInterface();
const char *dockDBusEntryInterface();

#endif  // FRAME_DBUS_DOCKDBUSNAMES_H_
