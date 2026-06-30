#!/usr/bin/env python3

# Copyright (C) 2026 CharOfString <markus_verify@126.com>
#
# This file is part of gxde-dock.
#
# gxde-dock is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gxde-dock is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gxde-dock.  If not, see <https://www.gnu.org/licenses/>.

"""
Replacement of daemon.go.

Usage: python3 -m dock
"""

import logging, signal, sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import dbus, dbus.mainloop.glib
from gi.repository import GLib

SERVICE_NAME = "top.gxde.dock.fallback.service.Main"
SESSION_MANAGER_SERVICE = "com.deepin.SessionManager"
log = logging.getLogger("dock")
log.setLevel(logging.DEBUG)
log_header = logging.StreamHandler()
log_header.setFormatter(logging.Formatter("(%(levelname)s) %(name)s: %(message)s"))
log.addHandler(log_header)

def main():
    from . import daemon as dock_daemon

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    GLib.set_prgname("gxde-dock-fallback-dock")
    dock_daemon.start()

    # Handle D-Bus
    bus = dbus.SessionBus()
    bus_name = dbus.service.BusName(SERVICE_NAME, bus)
    from .dock_manager import DockManager
    mgr = DockManager(bus, bus_name)
    log.info(f"Service named -> {SERVICE_NAME}")

    # USe GLib to exit gracefully.
    loop = GLib.MainLoop()

    def on_name_owner_changed(name, old_owner, new_owner):
        if name == SESSION_MANAGER_SERVICE and old_owner and not new_owner:
            log.info("Desktop session ended; stopping fallback service.")
            loop.quit()

    bus.add_signal_receiver(
        on_name_owner_changed,
        signal_name="NameOwnerChanged",
        dbus_interface="org.freedesktop.DBus",
        arg0=SESSION_MANAGER_SERVICE,
    )
    signal.signal(signal.SIGTERM, lambda *a: loop.quit())
    signal.signal(signal.SIGINT, lambda *a: loop.quit())
    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    dock_daemon.stop()
    log.info("Exited.")

if __name__ == "__main__":
    main()
