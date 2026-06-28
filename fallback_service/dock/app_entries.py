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
Translation of app_entries.go + bamf.go to Python 3.

AppEntries is a collection of AppEntry, and bamf.go is a map of PID to AppEntry.
"""

import logging

log = logging.getLogger("dock.AppEntries")

# ---------- bamf.go: maps PID to AppEntry ----------
_pid_entry_map = {}

def register_pid_entry(pid, entry):
    _pid_entry_map[pid] = entry

def unregister_pid(pid):
    _pid_entry_map.pop(pid, None)

def get_entry_by_pid(pid):
    return _pid_entry_map.get(pid)


# ---------- app_entries.go ----------
class AppEntries:
    def __init__(self):
        self._items = []
        self.insert_cb = None   # args: entry, index
        self.remove_cb = None   # args: entry

    def insert(self, entry, index):
        if entry in self._items:
            self._items.remove(entry)
        if index < 0 or index >= len(self._items):
            self._items.append(entry)
            idx = len(self._items) - 1
        else:
            self._items.insert(index, entry)
            idx = index
        if self.insert_cb:
            self.insert_cb(entry, idx)

    def remove(self, entry):
        if entry in self._items:
            self._items.remove(entry)
            if self.remove_cb:
                self.remove_cb(entry)

    def GetById(self, entry_id):
        for e in self._items:
            if e.id == entry_id:
                return e
        return None

    def GetByInnerId(self, inner_id):
        for e in self._items:
            if e.app_id == inner_id:
                return e
        return None

    def GetByDesktopFilePath(self, path):
        for e in self._items:
            if e._desktop_file == path:
                return e
        return None

    def FilterDocked(self):
        return [e for e in self._items if e._is_docked]

    def Move(self, idx, new_idx):
        if 0 <= idx < len(self._items) and 0 <= new_idx < len(self._items):
            e = self._items.pop(idx)
            self._items.insert(new_idx, e)
            return True
        return False

    def __iter__(self):
        return iter(self._items)

    def __len__(self):
        return len(self._items)
