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
Translation of app_entry.go + dock_dbusutil.go to Python 3

AppEntry: struct, fields, property setters/emitters.
DBus interface methods are in app_entry_interface.py, menu is in app_entry_menu.py.
"""

import logging

import dbus
import dbus.service

from typing import Any, Dict, Optional, Set
from .types import HideState
from .window import WindowInfo, WindowInfos, WindowSlice, get_window_pid, get_process_name
from .menu import Menu
from .app_info import AppInfo
from .app_entry_interface import AppEntryInterface
from .app_entry_menu import AppEntryMenu

log = logging.getLogger("dock.entry")

ENTRY_IFACE = "com.deepin.dde.daemon.Dock.Entry"
ENTRY_PATH_PREFIX = "/com/deepin/dde/daemon/Dock/entries/"

# bamf.go: window PID to AppEntry mapping
_window_pid_app_map: Dict[int, "AppEntry"] = {}


class AppEntry(AppEntryInterface, AppEntryMenu, dbus.service.Object):
    def __init__(self, manager, entry_id: str, app_id: str,
                 conn, bus_name):
        obj_path = ENTRY_PATH_PREFIX + entry_id
        super().__init__(conn, obj_path, bus_name)

        self.manager = manager
        self.id = entry_id
        self.app_id = app_id
        self._app_info: Optional[AppInfo] = None
        self._name = ""
        self._icon = ""
        self._desktop_file = ""
        self._is_active = False
        self._is_docked = False
        self._current_window = 0

        self._windows: Set[int] = set()
        self._window_infos = WindowInfos()
        self._window_slice = WindowSlice()

        self._menu: Optional[Menu] = None
        self._service = bus_name

        self._init_from_app_id()
        self.initMenu()

    def refreshMenu(self):
        self.initMenu()
        self._prop_changed("Menu", self._menu.generate_json())

    # Setters
    def _prop_changed(self, name: str, value: Any):
        self.PropertiesChanged(ENTRY_IFACE, {name: value}, [])

    def setPropId(self, val: str) -> bool:
        if self.id != val:
            self.id = val
            self._prop_changed("Id", val)
            return True
        return False

    def setPropName(self, val: str) -> bool:
        if self._name != val:
            self._name = val
            self._prop_changed("Name", val)
            return True
        return False

    def setPropIcon(self, val: str) -> bool:
        if self._icon != val:
            self._icon = val
            self._prop_changed("Icon", val)
            return True
        return False

    def setPropIsActive(self, val: bool) -> bool:
        if self._is_active != val:
            self._is_active = val
            self._prop_changed("IsActive", val)
            return True
        return False

    def setPropCurrentWindow(self, val: int) -> bool:
        if self._current_window != val:
            self._current_window = val
            self._prop_changed("CurrentWindow", val)
            return True
        return False

    def setPropIsDocked(self, val: bool) -> bool:
        if self._is_docked != val:
            self._is_docked = val
            self._prop_changed("IsDocked", val)
            return True
        return False

    def setPropWindowInfos(self, val: WindowInfos) -> bool:
        if self._window_infos != val:
            self._window_infos = val
            self._prop_changed("WindowInfos", val.to_dbus())
            return True
        return False

    def setPropDesktopFile(self, val: str) -> bool:
        if self._desktop_file != val:
            self._desktop_file = val
            self._prop_changed("DesktopFile", val)
            return True
        return False

    # ---------- app_entry.go ----------
    def _init_from_app_id(self):
        """Go: initInnerIdAndAppInfo()"""
        self._app_info = AppInfo.from_id(self.app_id)
        if self._app_info:
            self._name = self._app_info.get_name()
            self._icon = self._app_info.get_icon()
            self._desktop_file = self._app_info.get_file_name()

    def updateName(self):
        """Go: updateName()"""
        if self._app_info:
            self.setPropName(self._app_info.get_name())

    def addWindow(self, win: int):
        """Go: addWindow(win x.Window)"""
        self._windows.add(win)
        self._window_slice.move_to_last(win)
        self._updateWindowInfo(win)
        self._updateCurrentWindow()
        self._updateIsActive()
        self.refreshMenu()

    def removeWindow(self, win: int):
        """Go: removeWindow(win x.Window)"""
        self._windows.discard(win)
        self._window_slice.remove(win)
        removed = self._window_infos._data.pop(win, None)
        self._updateCurrentWindow()
        self._updateIsActive()
        self.refreshMenu()
        if removed is not None:
            self._prop_changed("WindowInfos", self._window_infos.to_dbus())

    def _updateWindowInfo(self, win: int):
        """Go: updateWindowInfo(win x.Window)"""
        info = WindowInfo(win=win)
        pid = get_window_pid(win)
        if pid > 0:
            info.pid = pid
            info.process_name = get_process_name(pid)
        self._window_infos[win] = info
        self._prop_changed("WindowInfos", self._window_infos.to_dbus())

    def _updateCurrentWindow(self):
        """Go: updateCurrentWindow()"""
        ws = self._window_slice
        if ws.len() > 0:
            self.setPropCurrentWindow(ws.get(ws.len() - 1))
        elif self._windows:
            self.setPropCurrentWindow(next(iter(self._windows)))
        else:
            self.setPropCurrentWindow(0)

    def _updateIsActive(self):
        """Go: updateIsActive()"""
        # Wayland: active if this entry owns the compositor-focused window
        active_win = self.manager._active_window
        if active_win > 0:
            self.setPropIsActive(active_win in self._windows)
        else:
            self.setPropIsActive(len(self._windows) > 0)

    def launchApp(self, timestamp: int, files: list):
        """Go: launchApp()"""
        if not self._desktop_file:
            log.warning(f"launchApp: no desktop file for entry {self.id} (app_id={self.app_id})")
            return

        log.info(f"launchApp: launching {self._desktop_file} for entry {self.id}")
        try:
            from gi.repository import Gio

            desktop_app = Gio.DesktopAppInfo.new_from_filename(self._desktop_file)
            if desktop_app is None:
                raise RuntimeError(f"invalid desktop file: {self._desktop_file}")
            uris = [Gio.File.new_for_commandline_arg(path).get_uri()
                    for path in files]
            ctx = Gio.AppLaunchContext()
            # Unset QT_WAYLAND_SHELL_INTEGRATION=layer-shell before launching anything
            ctx.setenv("QT_WAYLAND_SHELL_INTEGRATION", "")
            ctx.setenv("QT_QPA_PLATFORM", "")
            if not desktop_app.launch_uris(uris, ctx):
                raise RuntimeError("GIO rejected the launch request")
            log.info(f"launchApp: {self.id} → {desktop_app.get_executable()} launched OK")
        except Exception as e:
            log.warning(f"launch app failed: {e}")

    def launchDesktopAction(self, action: str, timestamp: int):
        """Prase desktop actions from .desktop file."""
        if not self._desktop_file:
            return
        try:
            from gi.repository import Gio

            desktop_app = Gio.DesktopAppInfo.new_from_filename(
                self._desktop_file)
            if desktop_app is not None and action in desktop_app.list_actions():
                context = Gio.AppLaunchContext()
                context.setenv("QT_WAYLAND_SHELL_INTEGRATION", "")
                context.setenv("QT_QPA_PLATFORM", "")
                desktop_app.launch_action(action, context)
                return
            raise RuntimeError(f"unknown desktop action: {action}")
        except Exception as error:
            log.warning(f"launch desktop action {action!r} failed: {error}")

    def desktopActions(self):
        """Return (action id, display name) pairs for the dock menu."""
        return self._app_info.get_actions() if self._app_info else []

    def getMenu(self):
        """Go: getMenu()"""
        return self._menu

    # D-Bus prop.
    def _props(self) -> Dict[str, Any]:
        return {
            "Id": self.id,
            "Name": self._name,
            "Icon": self._icon,
            "CurrentWindow": dbus.UInt32(self._current_window),
            "IsActive": self._is_active,
            "WindowInfos": self._window_infos.to_dbus(),
            "DesktopFile": self._desktop_file,
            "IsDocked": self._is_docked,
            "Menu": self._menu.generate_json() if self._menu else "",
        }

    @dbus.service.method(dbus.PROPERTIES_IFACE,
        in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface: str) -> Dict[str, Any]:
        return self._props() if iface == ENTRY_IFACE else {}

    @dbus.service.method(dbus.PROPERTIES_IFACE,
        in_signature="ss", out_signature="v")
    def Get(self, iface: str, prop: str) -> Any:
        return self._props().get(prop)

    @dbus.service.signal(dbus.PROPERTIES_IFACE, signature="sa{sv}as")
    def PropertiesChanged(self, iface: str, changed: Dict[str, Any],
            invalidated: list):
        pass


def _register_interface_methods():
    """Register AppEntryInterface D-Bus methods on AppEntry."""
    cls = AppEntry
    table = cls._dbus_class_table
    cls_name = cls.__module__ + '.' + cls.__qualname__
    cls_entry = table.setdefault(cls_name, {})
    iface_entry = cls_entry.setdefault(ENTRY_IFACE, {})

    for name in dir(AppEntryInterface):
        if name.startswith('_'):
            continue
        attr = getattr(AppEntryInterface, name, None)
        if not callable(attr):
            continue
        func = getattr(attr, '__func__', None)
        if func is None:
            func = attr
        if not getattr(func, '_dbus_is_method', False):
            continue
        # Store the decorated function directly on AppEntry
        setattr(cls, name, func)
        iface_entry[name] = func

_register_interface_methods()
