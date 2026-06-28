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
Translation of sync_config.go to Python 3.

Dsync config sync module. This is corresponding to dsync.NewDock()'s syncConfig.
Hot sync is done via Get()/Set() and they're linked with gxde-dock via DBus PropertiesChanged
and PluginSettingsSynced signals.
"""

import json
import os
import logging
from typing import Dict, Any, List

log = logging.getLogger("dock.SyncConfig")

SYNC_VERSION = "1.0"

class SyncConfig:
    """Go: syncConfig struct"""

    def __init__(self, manager):
        self.m = manager

    def diff_str_slice(self, a: List[str], b: List[str]) -> tuple:
        """Go: diffStrSlice()"""
        set_a = set(a)
        set_b = set(b)
        added = [x for x in b if x not in set_a]
        removed = [x for x in a if x not in set_b]
        return added, removed

    def get(self) -> dict:
        """Go: Get() -> syncData"""
        from .util import unzip_desktop_path
        v = {
            "version": SYNC_VERSION,
            "icon_size": self.m.icon_size,
            "display_mode": str(self.m._display_mode),
            "hide_mode": str(self.m._hide_mode),
            "position": str(self.m._position),
            "docked_apps": [e._desktop_file for e in self.m._entries if e._is_docked],
            "plugins": self.m._plugin._data,
        }
        return v

    def set_docked_apps(self, docked_apps: List[str]):
        """Go: setDockedApps()"""
        from .util import unzip_desktop_path
        current = [e._desktop_file for e in self.m._entries if e._is_docked]
        added, removed = self.diff_str_slice(current, docked_apps)

        for desktop_file in added:
            desktop_file = unzip_desktop_path(desktop_file)
            if os.path.isfile(desktop_file):
                self.m.request_dock(desktop_file, -1)

        for desktop_file in removed:
            desktop_file = unzip_desktop_path(desktop_file)
            self.m.request_undock(desktop_file)

    def set_plugin_settings(self, settings: dict):
        """Go: setPluginSettings()"""
        if self.m._plugin._data != settings:
            self.m._plugin._data = settings
            self.m._plugin._save()
            self.m.PluginSettingsSynced()

    def set_from_data(self, data: bytes):
        """Go: Set(data []byte)"""
        v = json.loads(data)
        m = self.m
        m.icon_size = int(v.get("icon_size", 36))
        m._display_mode = int(v.get("display_mode", 0))
        m._hide_mode = int(v.get("hide_mode", 0))
        m._position = int(v.get("position", 2))
        self.set_docked_apps(v.get("docked_apps", []))
        self.set_plugin_settings(v.get("plugins", {}))
