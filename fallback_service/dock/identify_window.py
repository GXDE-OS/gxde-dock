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
Translation of identify_window.go + identify_window_pattern.go to Python 3.

Window identification helper. Rules are defined in windowPatterns JSON configuration.
"""

import json
import logging
from typing import List, Dict, Optional, Callable

log = logging.getLogger("dock.Identify")

# IdentifyWindowFunc / registerIdentifyWindowFuncs
_identify_funcs: List[Callable[[int], Optional[str]]] = []

def register_identify_func(f: Callable[[int], Optional[str]]):
    _identify_funcs.append(f)


def identify_window(win: int) -> str:
    """Go: IdentifyWindow()"""
    for f in _identify_funcs:
        result = f(win)
        if result:
            return result
    return ""


# ---------- identify_window_pattern.go ----------
class WindowPattern:
    """Go: WindowPattern struct"""
    def __init__(self, data: dict):
        self.wm_class: List[str] = data.get("WMClass", [])
        self.wm_name: List[str] = data.get("WMName", [])
        self.wm_role: List[str] = data.get("WMRole", [])
        self.app_id: str = data.get("AppId", "")


class WindowPatterns:
    """Go: WindowPatterns struct"""

    def __init__(self):
        self._patterns: List[WindowPattern] = []

    def load(self, filepath: str):
        """Go: loadWindowPatterns()"""
        try:
            with open(filepath, "r") as f:
                data = json.load(f)
            for item in data:
                self._patterns.append(WindowPattern(item))
        except Exception as e:
            log.warning(f"loadWindowPatterns failed: {e}")

    def find(self, win_uuid):
        """Go: Find(win x.Window), REWRITE using WLR_TOPLEVEL"""
        from .toplevel import get_toplevel_info
        info = get_toplevel_info(str(win_uuid))
        if not info:
            return None

        app_id = info.app_id.lower() if info.app_id else ""
        title = info.title.lower() if info.title else ""

        for pat in self._patterns:
            # WMClass -> app_id matching
            if pat.wm_class:
                for cls_name in pat.wm_class:
                    if cls_name.lower() in app_id:
                        return pat
            # WMName -> title matching
            if pat.wm_name:
                for name in pat.wm_name:
                    if name.lower() in title:
                        return pat
            # WMRole -> title role matching
            if pat.wm_role:
                for role in pat.wm_role:
                    if role.lower() in title:
                        return pat

        return None
