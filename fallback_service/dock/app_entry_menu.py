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

from .menu import Menu, MenuItem

class AppEntryMenu:
    """Go: app_entry_menu.go/initMenu()"""

    def initMenu(self):
        """Go: initMenu()"""
        self._menu = Menu()
        m = self._menu

        m.append_item(MenuItem.new("Open", lambda ts: self.Activate(ts), True))
        m.append_item(MenuItem.new("All Windows",
            lambda ts: self.PresentWindows(), True))

        if not self._is_docked:
            m.append_item(MenuItem.new("Dock", lambda ts: self.RequestDock(), True))
        else:
            m.append_item(MenuItem.new("Undock",
                lambda ts: self.RequestUndock(), True))
