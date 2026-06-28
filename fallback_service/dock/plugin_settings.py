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
Translation of plugin_settings.go to Python 3.

1s delay has been added to ensure safety.
"""

import json
import logging
from threading import Lock, Timer
from typing import Any, Dict

log = logging.getLogger("dock.PluginSettings")

# See dock_manager_init.go
_SETTING_KEY = "plugin-settings"

class PluginSettingsStorage:
    """Go: pluginSettingsStorage"""

    def __init__(self, gsettings_obj):
        """
        gsettings_obj: Gio.Settings('com.deepin.dde.dock')/None
        Go: m.settings
        """
        self._gs = gsettings_obj
        self._data: Dict[str, Dict[str, Any]] = {}
        self._data_mu = Lock()
        self._save_state_mu = Lock()
        self._saving = False
        self._timer: Timer = Timer(3, self._on_timer)

        if self._gs is not None:
            json_str = self._gs_get(_SETTING_KEY, "{}")
        else:
            # Operating file directly while GIO is unavailable
            import os
            self._fallback_file = os.path.join(
                os.path.expanduser("~/.cache/gxde-dock/fallback"), "plugin_settings.json")
            try:
                with open(self._fallback_file, "r") as f:
                    json_str = f.read()
            except (FileNotFoundError, IOError):
                json_str = "{}"

        try:
            v = json.loads(json_str)
            if isinstance(v, dict):
                self._data = v
        except json.JSONDecodeError as e:
            log.warning("Failed to load plugin settings %s", e)
            self._data = {}

    def _gs_get(self, key, default):
        """Go: m.settings.GetString(key)"""
        try:
            return self._gs.get_string(key)
        except Exception:
            return default

    def _gs_set(self, key, value):
        """Go: m.settings.SetString(key, value)"""
        try:
            return self._gs.set_string(key, value)
        except Exception as e:
            log.warning("Failed to save plugin settings %s", e)
            return False
        return True

    # Go: requestSave() + save()

    def request_save(self):
        """Go: requestSave()"""
        with self._save_state_mu:
            if self._saving:
                return
            self._timer.cancel()
            self._timer = Timer(1, self._on_timer)
            self._timer.start()
            self._saving = True

    def _on_timer(self):
        """Go: save()"""
        self.save()
        with self._save_state_mu:
            self._saving = False

    def save(self):
        """Go: save()"""
        with self._data_mu:
            json_str = json.dumps(self._data, ensure_ascii=False)
        if self._gs is not None:
            self._gs_set(_SETTING_KEY, json_str)
        else:
            import os
            os.makedirs(os.path.dirname(self._fallback_file), exist_ok=True)
            try:
                with open(self._fallback_file, "w") as f:
                    f.write(json_str)
            except IOError as e:
                log.warning("Failed to save plugin settings %s", e)

    # Go: Manager's D-Bus methods

    def get_json_str(self) -> str:
        """Go: getJsonStr() / GetPluginSettings()"""
        with self._data_mu:
            return json.dumps(self._data)

    def set(self, v: dict):
        """Go: set() → SetPluginSettings()"""
        with self._data_mu:
            self._data = v
        self.request_save()

    def merge(self, v: dict):
        """Go: merge() → MergePluginSettings()"""
        with self._data_mu:
            for key1, value1 in v.items():
                if key1 not in self._data and value1:
                    self._data[key1] = {}
                if isinstance(value1, dict):
                    for key2, value2 in value1.items():
                        self._data[key1][key2] = value2
        self.request_save()

    def remove(self, key1: str, key2_list: list):
        """Go: remove() → RemovePluginSettings()"""
        with self._data_mu:
            if not key2_list:
                self._data.pop(key1, None)
            else:
                value1 = self._data.get(key1)
                if isinstance(value1, dict):
                    for key2 in key2_list:
                        value1.pop(key2, None)
                    if not value1:
                        del self._data[key1]
        self.request_save()

    def equal(self, v: dict) -> bool:
        """Go: equal() — 深度比较"""
        with self._data_mu:
            return self._data == v
