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
Translation of app_entry_ifc.go to Python 3.

AppEntry D-Bus interface method: Activate, NewInstance, HandleMenuItem, etc.
"""

import logging
from typing import List

import dbus
import dbus.service

from .util import get_current_timestamp

log = logging.getLogger("dock.EntryIfc")

ENTRY_IFACE = "com.deepin.dde.daemon.Dock.Entry"

class AppEntryInterface:
    """Go: app_entry_ifc.go's D-Bus methods"""
    # u = UINT32
    # s = STRING
    # uas = u + as = UINT32 + ARRAY of STRING

    @dbus.service.method(ENTRY_IFACE, in_signature="u", out_signature="")
    def Activate(self, timestamp: int):
        import time as _time
        _start = _time.time()
        log.info(f"Activate entry {self.id} windows={self._windows} current={self._current_window}")
        if self._windows:
            win = self._current_window or next(iter(self._windows))
            self.manager.ActivateWindow(win)
            log.info(f"Activate entry {self.id} -> window {win} done within {(_time.time()-_start)*1000:.0f}ms")
        else:
            self.launchApp(timestamp, [])
            log.info(f"Activate entry {self.id} -> launchApp done within {(_time.time()-_start)*1000:.0f}ms")

    @dbus.service.method(ENTRY_IFACE, in_signature="u", out_signature="")
    def NewInstance(self, timestamp: int):
        log.info(f"NewInstance entry {self.id}")
        self.launchApp(timestamp, [])

    @dbus.service.method(ENTRY_IFACE, in_signature="us", out_signature="")
    def HandleMenuItem(self, timestamp: int, item_id: str):
        if self._menu:
            self._menu.handle_action(item_id, timestamp)

    @dbus.service.method(ENTRY_IFACE, in_signature="uas", out_signature="")
    def HandleDragDrop(self, timestamp: int, files: List[str]):
        self.launchApp(timestamp, files)

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="")
    def PresentWindows(self):
        pass

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="")
    def RequestDock(self):
        self.setPropIsDocked(True)
        self.refreshMenu()

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="")
    def RequestUndock(self):
        self.setPropIsDocked(False)
        self.refreshMenu()

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="")
    def Check(self):
        self._updateIsActive()

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="au")
    def GetAllowedCloseWindows(self) -> List[int]:
        return list(self._windows)

    @dbus.service.method(ENTRY_IFACE, in_signature="", out_signature="")
    def ForceQuit(self):
        for win in list(self._windows):
            self.manager.CloseWindow(win)
