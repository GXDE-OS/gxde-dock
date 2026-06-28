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
Translation of app_info.go to Python 3.

Prase .desktop file and generate inner_id
"""

import os
import re
import hashlib
import configparser
from typing import Optional

from .util import add_desktop_ext

DESKTOP_HASH_PREFIX = "d:"
DESKTOP_SECTION = "Desktop Entry"


class AppInfo:
    """Go: AppInfo struct"""

    def __init__(self, desktop_file: str):
        self._file = desktop_file
        self._data: dict = {}
        self.name = ""
        self.icon = ""
        self.id = ""
        self.commandline = ""
        self.inner_id = ""
        self.identify_method = ""
        self._loaded = False

    # Go: desktopappinfo.DesktopAppInfo
    @classmethod
    def from_file(cls, path: str) -> Optional["AppInfo"]:
        """Go: NewAppInfoFromFile()"""
        if not path:
            return None
        if not os.path.isfile(path):
            return None
        result = cls(path)
        result._load()
        return result

    @classmethod
    def from_id(cls, app_id: str) -> Optional["AppInfo"]:
        """Go: NewAppInfo()"""
        if not app_id:
            return None

        # Result like "firefox.desktop"
        if "/" in app_id:
            return cls.from_file(app_id)

        # Standard path
        for base in ("/usr/share/applications", "/usr/local/share/applications",
                os.path.expanduser("~/.local/share/applications")):
            path = os.path.join(base, add_desktop_ext(app_id))
            if os.path.isfile(path):
                return cls.from_file(path)
        return None

    def _load(self):
        if self._loaded:
            return
        try:
            cp = configparser.ConfigParser()
            cp.read(self._file, encoding="utf-8")
            if DESKTOP_SECTION not in cp:
                return
            sec = cp[DESKTOP_SECTION]
        except Exception:
            return
        self._loaded = True

        self.id = os.path.basename(self._file)

        x_vendor = sec.get("X-Deepin-Vendor", "")
        if x_vendor == "deepin":
            self.name = sec.get("GenericName", "") or sec.get("Name", "")
        else:
            self.name = sec.get("Name", "")

        self.icon = sec.get("Icon", "")
        try:
            self.commandline = sec.get("Exec", "")
        except Exception:
            self.commandline = ""
        self.identify_method = sec.get("X-Deepin-AppID", "")

        # Go: genInnerId() -> md5(commandline)
        self.inner_id = DESKTOP_HASH_PREFIX + hashlib.md5(
            self.commandline.encode("utf-8")
        ).hexdigest()

    # Some getters
    def get_id(self) -> str:
        return self.id

    def get_name(self) -> str:
        return self.name

    def get_icon(self) -> str:
        return self.icon

    def get_file_name(self) -> str:
        """Go: GetFileName()"""
        return self._file

    def get_commandline(self) -> str:
        return self.commandline

    def is_installed(self) -> bool:
        """Go: IsInstalled()"""
        return os.path.isfile(self._file)

    def __repr__(self):
        return (f"<AppInfo id={self.id!r} hash={self.inner_id!r} "
            f"icon={self.icon!r} desktop={self._file!r}>")
