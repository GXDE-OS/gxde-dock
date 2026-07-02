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
Translation of app_entry_menu.go to Python 3

AppEntry right click menu's initialization.
"""

import gettext

from .menu import Menu, MenuItem
from .locale_utils import ui_locale_fallbacks


_translate = gettext.translation(
    "dde-daemon", localedir="/usr/share/locale",
    languages=ui_locale_fallbacks() or None, fallback=True).gettext

class AppEntryMenu:
    """Go: app_entry_menu.go/initMenu()"""

    def initMenu(self):
        """Go: initMenu()"""
        self._menu = Menu()
        m = self._menu

        m.append_item(MenuItem.new(
            _translate("_Open"), lambda ts: self.Activate(ts), True))

        # Docked app icon: Open and undock
        # Opened app: Window actions, dock/undock, close all, force kill...
        if not self._windows:
            if self._is_docked:
                m.append_item(MenuItem.new(
                    _translate("_Undock"),
                    lambda ts: self.RequestUndock(), True))
            else:
                m.append_item(MenuItem.new(
                    _translate("_Dock"),
                    lambda ts: self.RequestDock(), True))
            return

        m.append_item(MenuItem.new(
            _translate("_All windows"),
            lambda ts: self.PresentWindows(), True))

        for action, name in self.desktopActions():
            m.append_item(MenuItem.new(
                name,
                lambda ts, action_id=action:
                    self.launchDesktopAction(action_id, ts),
                True))

        if not self._is_docked:
            m.append_item(MenuItem.new(
                _translate("_Dock"), lambda ts: self.RequestDock(), True))
        else:
            m.append_item(MenuItem.new(_translate("_Undock"),
                lambda ts: self.RequestUndock(), True))

        m.append_item(MenuItem.new(
            _translate("_Force Quit"), lambda ts: self.ForceQuit(), True))
