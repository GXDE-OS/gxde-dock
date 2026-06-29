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
Translation of dock_manager.go + dock_manager_init.go + dock_manager_entries.go +
dock_manager_dock_app.go + dock_manager_hide_state.go + dock_manager_xevent.go to Python 3.

Note that X11 support has been dropped.
Impl. of com.deepin.dde.daemon.Dock.
"""

import os
import json
import logging
from collections import OrderedDict
from typing import Any, Dict, List, Optional

import dbus
import dbus.service
from gi.repository import Gio, GLib  # type: ignore

from .types import HideMode, HideState, DisplayMode, Position, Rect
from .util import (
    init_xdg_autostart_dirs,
    init_path_dir_code_map,
    to_local_path,
    unzip_desktop_path,
    zip_desktop_path,
)
from . import toplevel as _wl
from .window import WindowInfos
from .menu import Menu
from .app_info import AppInfo
from .app_entry import AppEntry, ENTRY_PATH_PREFIX
from .app_entries import AppEntries
from .sync_config import SyncConfig
from .plugin_settings import PluginSettingsStorage

log = logging.getLogger("dock.manager")

DBUS_IFACE = "com.deepin.dde.daemon.Dock"
DBUS_PATH = "/com/deepin/dde/daemon/Dock"

DOCK_SCHEMA = "com.deepin.dde.dock"
APPEARANCE_SCHEMA = "com.deepin.dde.appearance"

KEY_HIDE_MODE = "hide-mode"
KEY_DISPLAY_MODE = "display-mode"
KEY_POSITION = "position"
KEY_ICON_SIZE = "icon-size"
KEY_DOCKED_APPS = "docked-apps"
KEY_SHOW_TIMEOUT = "show-timeout"
KEY_HIDE_TIMEOUT = "hide-timeout"
KEY_WINDOW_SPLIT = "window-split"
KEY_OPACITY = "opacity"

FRONTEND_WM_CLASS = "dde-dock"

DBUS_SERVICE_NAME = "com.deepin.dde.daemon.Dock"  # 原始名，用于 internal export

DATA_DIR = os.path.expanduser("~/.cache/gxde-dock/fallback")
HOME_DIR = os.path.expanduser("~")

DISPLAY_MODE_FROM_GSETTINGS = {
    "fashion": DisplayMode.Fashion,
    "efficient": DisplayMode.Efficient,
    "classic": DisplayMode.Classic,
}
DISPLAY_MODE_TO_GSETTINGS = {
    DisplayMode.Fashion: "fashion",
    DisplayMode.Efficient: "efficient",
    DisplayMode.Classic: "classic",
}
HIDE_MODE_FROM_GSETTINGS = {
    "keep-showing": HideMode.KeepShowing,
    "keep-hidden": HideMode.KeepHidden,
    "smart-hide": HideMode.SmartHide,
}
HIDE_MODE_TO_GSETTINGS = {
    HideMode.KeepShowing: "keep-showing",
    HideMode.KeepHidden: "keep-hidden",
    HideMode.SmartHide: "smart-hide",
}
POSITION_FROM_GSETTINGS = {
    "top": Position.Top,
    "right": Position.Right,
    "bottom": Position.Bottom,
    "left": Position.Left,
}
POSITION_TO_GSETTINGS = {
    Position.Top: "top",
    Position.Right: "right",
    Position.Bottom: "bottom",
    Position.Left: "left",
}


class DockManager(dbus.service.Object):
    """Go: Manager struct"""

    def __init__(self, conn, bus_name):
        super().__init__(conn, DBUS_PATH, bus_name)
        self._conn = conn
        self._bus_name = bus_name

        os.makedirs(DATA_DIR, exist_ok=True)

        # Init
        init_path_dir_code_map(HOME_DIR)
        init_xdg_autostart_dirs()

        # GSettings
        self._gs: Optional[Gio.Settings] = None
        self._gs_appearance: Optional[Gio.Settings] = None
        try:
            self._gs = Gio.Settings.new(DOCK_SCHEMA)
            self._gs_appearance = Gio.Settings.new(APPEARANCE_SCHEMA)
        except Exception as e:
            log.warning(f"GSettings unavailable: {e}")

        # Manager inner state
        self._entries = AppEntries()
        self._entries.insert_cb = self._on_entry_added
        self._entries.remove_cb = self._on_entry_removed
        self._entry_counter = 0
        self._plugin = PluginSettingsStorage(self._gs)
        self._sync_config = SyncConfig(self)
        self._frontend_rect = Rect()
        self._hide_state = HideState.Show
        self._client_list: List[int] = []
        self._active_window = 0

        # Prop cache via GSettings
        self.icon_size = self._gs_get(KEY_ICON_SIZE, 36, "u")
        self._display_mode = self._gs_get_enum(
            KEY_DISPLAY_MODE, DisplayMode.Fashion, DISPLAY_MODE_FROM_GSETTINGS)
        self._hide_mode = self._gs_get_enum(
            KEY_HIDE_MODE, HideMode.KeepShowing, HIDE_MODE_FROM_GSETTINGS)
        self._position = self._gs_get_enum(
            KEY_POSITION, Position.Bottom, POSITION_FROM_GSETTINGS)
        self._show_timeout = self._gs_get(KEY_SHOW_TIMEOUT, 300, "u")
        self._hide_timeout = self._gs_get(KEY_HIDE_TIMEOUT, 1000, "u")
        self._window_split = self._gs_get(KEY_WINDOW_SPLIT, False, "b")

        # Type Classic has been force migrated to Efficient
        if self._display_mode == DisplayMode.Classic:
            self._display_mode = DisplayMode.Efficient

        # initDockedApps() + initClientList()
        self._load_docked_apps()
        self._load_client_list()

        # Waylang things
        GLib.idle_add(self._dispatch_wayland)
        _wl.on_toplevel_created(self._on_wl_toplevel_created)
        _wl.on_toplevel_updated(self._on_wl_toplevel_updated)
        _wl.on_toplevel_closed(self._on_wl_toplevel_closed)

        log.info(f"DockManager ready @{DBUS_PATH}")

    # ---------- GSettings helpers ----------
    def _gs_get(self, key: str, default, variant_type: str):
        if self._gs is None:
            return default
        try:
            val = self._gs.get_value(key).unpack()
            return val
        except Exception:
            return default

    def _gs_get_enum(self, key: str, default, table: Dict[str, Any]):
        val = self._gs_get(key, default, "s")
        if isinstance(val, str):
            return table.get(val, default)
        try:
            return type(default)(int(val))
        except Exception:
            return default

    def _gs_set(self, key: str, variant_type: str, value):
        if self._gs is None:
            return
        try:
            self._gs.set_value(key, GLib.Variant(variant_type, value))
        except Exception as e:
            log.warning(f"_gs_set {key}={value}: {e}")

    # Go: initDockedApps
    def _load_docked_apps(self):
        docked = self._gs_get(KEY_DOCKED_APPS, [], "as")
        for app in docked:
            self._request_dock_internal(app, -1)

    def _load_client_list(self):
        """Go: initClientList() Now no need"""

    def _request_dock_internal(self, desktop_file: str, index: int) -> Optional[AppEntry]:
        """Go: requestDock()"""
        desktop_file = to_local_path(desktop_file)
        search_id = unzip_desktop_path(desktop_file)

        app_info = AppInfo.from_file(desktop_file)
        if not app_info:
            return None

        entry = self._entries.GetByInnerId(app_info.inner_id)
        if not entry:
            entry_id = f"e{self._entry_counter:x}"
            self._entry_counter += 1
            entry = AppEntry(self, entry_id, app_info.id,
                             self._conn, self._bus_name)
        entry._is_docked = True
        entry._app_info = app_info

        if entry not in self._entries:
            self._entries.insert(entry, index)

        self._save_docked_apps()
        return entry

    def _save_docked_apps(self):
        files = [zip_desktop_path(e._desktop_file)
                 for e in self._entries.FilterDocked()]
        self._gs_set(KEY_DOCKED_APPS, "as", files)

    # Pub. methods
    def request_dock(self, desktop_file: str, index: int) -> bool:
        return self._request_dock_internal(desktop_file, index) is not None

    def request_undock(self, desktop_file: str) -> bool:
        desktop_file = to_local_path(desktop_file)
        entry = self._entries.GetByDesktopFilePath(desktop_file)
        if entry:
            entry._is_docked = False
            if not entry._is_active:
                self._entries.remove(entry)
            self._save_docked_apps()
            return True
        return False

    # Callbacks
    def _on_entry_added(self, entry: AppEntry, index: int):
        obj_path = ENTRY_PATH_PREFIX + entry.id
        log.info(f"EntryAdded {entry.id} at {index}")
        self.EntryAdded(dbus.ObjectPath(obj_path), index)

    def _on_entry_removed(self, entry: AppEntry):
        log.info(f"EntryRemoved {entry.id}")
        self.EntryRemoved(entry.id)

    # Wayland replacement for xevent
    def _dispatch_wayland(self):
        _wl.dispatch()
        return True

    # Wl to entry mapping
    _uuid_to_entry = {}  # uuid (str) -> AppEntry

    def _on_wl_toplevel_created(self, info):
        """New window: create AppEntry"""
        entry = self._find_or_create_entry_by_app_id(info.app_id)
        if entry:
            self._uuid_to_entry[info.uuid] = entry
            entry.addWindow(info.uuid)

    def _on_wl_toplevel_updated(self, info):
        """Prop change -> update entry's WindowInfos"""
        entry = self._uuid_to_entry.get(info.uuid)
        if entry:
            from .window import WindowInfo
            wi = WindowInfo(
                win=hash(info.uuid) & 0xFFFFFFFF,
                title=info.title,
                app_id=info.app_id,
                pid=info.pid,
                state=info.state,
            )
            entry._window_infos[info.uuid] = wi
            entry._updateIsActive()

    def _on_wl_toplevel_closed(self, uuid):
        """Window closed -> remove from Entry"""
        entry = self._uuid_to_entry.pop(uuid, None)
        if entry:
            entry.removeWindow(uuid)

    def _find_or_create_entry_by_app_id(self, app_id):
        """Find/create AppEntry by app_id."""
        if not app_id:
            return None

        # Skip dock itself and desktop components
        if app_id == "gxde-dock" or app_id.startswith("dde-desktop"):
            return None
        for e in self._entries:
            if e._desktop_file and app_id in e._desktop_file:
                return e

        # Only create entries for apps that have a known desktop file
        from .app_info import AppInfo
        ai = AppInfo.from_id(app_id)
        if not ai:
            return None
        eid = f"w{self._entry_counter:x}"
        self._entry_counter += 1
        from .app_entry import AppEntry
        entry = AppEntry(self, eid, app_id, self._conn, self._bus_name)
        entry._app_info = ai
        entry._desktop_file = ai.get_file_name()
        entry._name = ai.get_name()
        entry._icon = ai.get_icon()
        self._entries.insert(entry, -1)
        return entry

    # Signals
    @dbus.service.signal(DBUS_IFACE)
    def ServiceRestarted(self):
        pass

    @dbus.service.signal(DBUS_IFACE, signature="oi")
    def EntryAdded(self, path: dbus.ObjectPath, index: int):
        pass

    @dbus.service.signal(DBUS_IFACE, signature="s")
    def EntryRemoved(self, entry_id: str):
        pass

    @dbus.service.signal(DBUS_IFACE)
    def PluginSettingsSynced(self):
        pass

    # Props
    def _get_all_props(self, iface: str) -> Dict[str, Any]:
        if iface != DBUS_IFACE:
            return {}
        entries = [dbus.ObjectPath(ENTRY_PATH_PREFIX + e.id)
                   for e in self._entries]
        return OrderedDict([
            ("Entries", entries),
            ("HideMode", dbus.Int32(self._hide_mode)),
            ("DisplayMode", dbus.Int32(self._display_mode)),
            ("Position", dbus.Int32(self._position)),
            ("IconSize", dbus.UInt32(self.icon_size)),
            ("ShowTimeout", dbus.UInt32(self._show_timeout)),
            ("HideTimeout", dbus.UInt32(self._hide_timeout)),
            ("WindowSplit", self._window_split),
            ("DockedApps", [e._desktop_file for e in self._entries.FilterDocked()]),
            ("Opacity", self._gs_get_opacity()),
            ("HideState", dbus.Int32(self._hide_state.value)),
            ("FrontendWindowRect", (
                dbus.Int32(self._frontend_rect.x),
                dbus.Int32(self._frontend_rect.y),
                dbus.UInt32(self._frontend_rect.width),
                dbus.UInt32(self._frontend_rect.height),
            )),
        ])

    def _gs_get_opacity(self):
        if self._gs_appearance:
            try:
                return self._gs_appearance.get_value(KEY_OPACITY).unpack()
            except Exception:
                pass
        return 1.0

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface: str) -> Dict[str, Any]:
        return self._get_all_props(iface)

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="ss", out_signature="v")
    def Get(self, iface: str, prop: str) -> Any:
        return self._get_all_props(iface).get(prop)

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="ssv", out_signature="")
    def Set(self, iface: str, prop: str, value: Any):
        log.info(f"Set {prop}={value}")
        try:
            if prop == "HideMode":
                self._hide_mode = int(value)
                self._gs_set(KEY_HIDE_MODE, "s",
                             HIDE_MODE_TO_GSETTINGS.get(
                                 HideMode(int(value)), "keep-showing"))
            elif prop == "DisplayMode":
                self._display_mode = int(value)
                self._gs_set(KEY_DISPLAY_MODE, "s",
                             DISPLAY_MODE_TO_GSETTINGS.get(
                                 DisplayMode(int(value)), "fashion"))
            elif prop == "Position":
                self._position = int(value)
                self._gs_set(KEY_POSITION, "s",
                             POSITION_TO_GSETTINGS.get(
                                 Position(int(value)), "bottom"))
            elif prop == "IconSize":
                self.icon_size = int(value)
                self._gs_set(KEY_ICON_SIZE, "u", int(value))
            elif prop == "ShowTimeout":
                self._show_timeout = int(value)
                self._gs_set(KEY_SHOW_TIMEOUT, "u", int(value))
            elif prop == "HideTimeout":
                self._hide_timeout = int(value)
                self._gs_set(KEY_HIDE_TIMEOUT, "u", int(value))
            elif prop == "WindowSplit":
                self._window_split = bool(value)
                self._gs_set(KEY_WINDOW_SPLIT, "b", bool(value))
            elif prop == "DockedApps":
                self._sync_config.set_docked_apps(list(value))
            elif prop == "Opacity":
                if self._gs_appearance:
                    self._gs_appearance.set_value(
                        KEY_OPACITY, GLib.Variant("d", float(value)))
            self.PropertiesChanged(DBUS_IFACE, {prop: value}, [])
        except Exception as e:
            log.warning(f"Set property failed: {e}")

    @dbus.service.signal(dbus.PROPERTIES_IFACE, signature="sa{sv}as")
    def PropertiesChanged(self, iface: str, changed: Dict[str, Any],
                          invalidated: List[str]):
        pass

    # Methods
    @dbus.service.method(DBUS_IFACE, in_signature="uuuu", out_signature="")
    def SetFrontendWindowRect(self, x: int, y: int, width: int, height: int):
        self._frontend_rect = Rect(x, y, width, height)
        self.PropertiesChanged(DBUS_IFACE, {
            "FrontendWindowRect": (
                dbus.Int32(x), dbus.Int32(y),
                dbus.UInt32(width), dbus.UInt32(height),
            )}, [])

    # Plugin settings
    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="s")
    def GetPluginSettings(self) -> str:
        return self._plugin.get_json_str()

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="")
    def SetPluginSettings(self, json_str: str):
        self._plugin.set_json_str(json_str)

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="")
    def MergePluginSettings(self, json_str: str):
        try:
            self._plugin.merge(json.loads(json_str))
        except json.JSONDecodeError:
            pass

    @dbus.service.method(DBUS_IFACE, in_signature="sas", out_signature="")
    def RemovePluginSettings(self, key1: str, key2_list: List[str]):
        self._plugin.remove(key1, key2_list)

    # Entry management
    @dbus.service.method(DBUS_IFACE, in_signature="si", out_signature="b")
    def RequestDock(self, desktop_file: str, index: int) -> bool:
        return self.request_dock(desktop_file, index)

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="b")
    def RequestUndock(self, desktop_file: str) -> bool:
        return self.request_undock(desktop_file)

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="b")
    def IsDocked(self, desktop_file: str) -> bool:
        return self._entries.GetByDesktopFilePath(
            to_local_path(desktop_file)) is not None

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="b")
    def IsOnDock(self, desktop_file: str) -> bool:
        return self.IsDocked(desktop_file)

    @dbus.service.method(DBUS_IFACE, in_signature="ii", out_signature="")
    def MoveEntry(self, index: int, new_index: int):
        self._entries.move(index, new_index)
        self._save_docked_apps()

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="as")
    def GetEntryIDs(self) -> List[str]:
        return [e.id for e in self._entries]

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="as")
    def GetDockedAppsDesktopFiles(self) -> List[str]:
        return [e._desktop_file for e in self._entries.FilterDocked()]

    # Window management
    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def ActivateWindow(self, win: int):
        _wl.activate_window(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def CloseWindow(self, win: int):
        _wl.close_window(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def MaximizeWindow(self, win: int):
        self.ActivateWindow(win)
        _wl.maximize_window(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def MinimizeWindow(self, win: int):
        _wl.minimize_window(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def MakeWindowAbove(self, win: int):
        self.ActivateWindow(win)
        _wl.make_window_above(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def MoveWindow(self, win: int):
        self.ActivateWindow(win)
        _wl.move_window(str(win))

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="")
    def PreviewWindow(self, win: int):
        log.info(f"PreviewWindow({win}) — no WM integration")

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="")
    def CancelPreviewWindow(self):
        pass

    @dbus.service.method(DBUS_IFACE, in_signature="u", out_signature="s")
    def QueryWindowIdentifyMethod(self, win: int) -> str:
        return ""
