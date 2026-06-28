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
Translation of util.go + desktop_file_path.go to Python 3.
X11 support has been dropped for this is a Wayland service.

Some utility functions for desktop file path handling, XDG autostart dirs, etc.
"""

import os
import shutil
import time
import base64
import dbus
import dbus.service

from pathlib import Path
from typing import List

# ---------- util.go ----------
XDG_AUTOSTART_DIRS: List[str] = []

def init_xdg_autostart_dirs():
    """Go: init()"""
    global XDG_AUTOSTART_DIRS
    config_dirs = []
    home = os.environ.get("HOME", "/home")
    config_dirs.append(os.path.join(home, ".config"))
    for d in ("/etc/xdg",):
        config_dirs.append(d)
    XDG_AUTOSTART_DIRS = [os.path.join(d, "autostart") for d in config_dirs]


def is_in_autostart_dir(filepath: str) -> bool:
    d = os.path.dirname(filepath)
    return d in XDG_AUTOSTART_DIRS


def data_uri_to_file(data_uri: str, path: str) -> str:
    comma = data_uri.find(",")
    if comma < 0:
        return path
    img_data = base64.b64decode(data_uri[comma + 1:])
    with open(path, "wb") as f:
        f.write(img_data)
    return path


def str_slice_equal(a: List[str], b: List[str]) -> bool:
    return a == b


def uniq_str_slice(s: List[str]) -> List[str]:
    seen = set()
    result = []
    for e in s:
        if e not in seen:
            seen.add(e)
            result.append(e)
    return result


def str_slice_contains(s: List[str], v: str) -> bool:
    return v in s


def copy_file_contents(src: str, dst: str):
    shutil.copy2(src, dst)


def get_current_timestamp() -> int:
    return int(time.time())


def to_local_path(in_str: str) -> str:
    from urllib.parse import urlparse
    u = urlparse(in_str)
    if u.scheme == "file":
        return u.path
    return in_str


def add_dir_trailing_slash(d: str) -> str:
    if not d:
        raise ValueError("EMPTY DIR PATH")
    if not d.endswith("/"):
        d += "/"
    return d


# ---------- desktop_file_path.go ----------
DESKTOP_EXTNAME = ".desktop"
PATH_DIR_CODE_MAP: dict = {}
PATH_CODE_DIR_MAP: dict = {}

def init_path_dir_code_map(home_dir: str):
    """Go: initPathDirCodeMap()"""
    global PATH_DIR_CODE_MAP, PATH_CODE_DIR_MAP
    PATH_DIR_CODE_MAP = {
        "/usr/share/applications/": "/S@",
        "/usr/local/share/applications/": "/L@",
    }
    d = add_dir_trailing_slash(os.path.join(home_dir, ".local/share/applications"))
    PATH_DIR_CODE_MAP[d] = "/H@"

    scratch = add_dir_trailing_slash(
        os.path.join(os.environ.get("XDG_CONFIG_HOME",
            os.path.join(home_dir, ".config")), "dock/scratch")
    )
    PATH_DIR_CODE_MAP[scratch] = "/D@"

    PATH_CODE_DIR_MAP = {v: k for k, v in PATH_DIR_CODE_MAP.items()}


def get_desktop_id_by_file_path(path: str) -> str:
    desktop_id = ""
    for d in PATH_DIR_CODE_MAP:
        if path.startswith(d):
            desktop_id = path[len(d):].replace("/", "-")
            break
    return desktop_id


def add_desktop_ext(s: str) -> str:
    return s if s.endswith(DESKTOP_EXTNAME) else s + DESKTOP_EXTNAME


def trim_desktop_ext(s: str) -> str:
    return s[:-len(DESKTOP_EXTNAME)] if s.endswith(DESKTOP_EXTNAME) else s


def zip_desktop_path(path: str) -> str:
    for d, code in PATH_DIR_CODE_MAP.items():
        if path.startswith(d):
            path = code + path[len(d):]
            break
    return trim_desktop_ext(path)


def unzip_desktop_path(path: str) -> str:
    if len(path) >= 3:
        head = path[:3]
        for code, d in PATH_CODE_DIR_MAP.items():
            if code == head:
                path = d + path[3:]
                break
    return add_desktop_ext(path)
