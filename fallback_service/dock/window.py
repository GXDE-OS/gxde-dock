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
Translation of window_info.go + window_infos.go + window_slice.go + process_info.go to Python 3
This translation is modified to support Wayland.

Wayland window info management w/ windows slice sorting and retrieval.
"""

import os
import logging
import dbus
from dataclasses import dataclass, field

log = logging.getLogger("dock.Window")


@dataclass
class WindowInfo:
    """Go: WindowInfo struct"""
    win: int = 0
    title: str = ""
    process_name: str = ""
    icon: str = ""
    flash: bool = False
    pid: int = 0
    app_id: str = ""
    state: int = 0    # gxde-wlcom toplevel state

    def to_dict(self):
        return {
            "Title": self.title or self.process_name,
            "Flash": self.flash,
        }

    @property
    def is_active(self):
        return bool(self.state & 4)

    @property
    def is_minimized(self):
        return bool(self.state & 2)


class WindowInfos:
    """Go: windowInfosType"""
    def __init__(self):
        self._data = {}

    def __getitem__(self, key):
        return self._data.get(key, WindowInfo(win=key))

    def __setitem__(self, key, value):
        self._data[key] = value

    def __contains__(self, key):
        return key in self._data

    def __eq__(self, other):
        return isinstance(other, WindowInfos) and self._data == other._data

    def to_dict(self):
        return {str(k): v.to_dict() for k, v in self._data.items()}

    def to_dbus(self):
        return dbus.Dictionary({
            dbus.UInt32(k): dbus.Struct(
                (dbus.Boolean(v.flash), dbus.String(v.title or v.process_name)),
                signature="bs")
            for k, v in self._data.items()
        }, signature="u(bs)")


class WindowSlice:
    """Go: windowSlice"""
    def __init__(self):
        self._list = []

    def move_to_last(self, win):
        if win in self._list:
            self._list.remove(win)
        self._list.append(win)

    def remove(self, win):
        try:
            self._list.remove(win)
        except ValueError:
            pass

    def get(self, index):
        return self._list[index] if 0 <= index < len(self._list) else 0

    def len(self):
        return len(self._list)

    def all(self):
        return list(self._list)


def get_process_name(pid):
    try:
        with open(f"/proc/{pid}/comm") as f:
            return f.read().strip()
    except Exception:
        return ""


def get_window_pid(win):
    """从 Wayland toplevel 获取 PID"""
    from .toplevel import get_toplevel_info
    info = get_toplevel_info(str(win))
    return info.pid if info else 0
