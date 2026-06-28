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
Translation of menu.go to Python 3.

JSON format right-click menu, supporting submenus.
"""

import json
from typing import List, Optional, Callable

class MenuItem:
    """Go: MenuItem struct"""
    __slots__ = ("id", "text", "is_active", "is_checkable", "checked",
                 "icon", "icon_hover", "icon_inactive", "show_check_mark",
                 "sub_menu", "hint", "action")

    def __init__(self):
        self.id = ""
        self.text = ""
        self.is_active = True
        self.is_checkable = False
        self.checked = False
        self.icon = ""
        self.icon_hover = ""
        self.icon_inactive = ""
        self.show_check_mark = False
        self.sub_menu: Optional["Menu"] = None
        self.hint = 0
        self.action: Optional[Callable[[int], None]] = None

    def to_dict(self) -> dict:
        d = {
            "itemId": self.id,
            "itemText": self.text,
            "isActive": self.is_active,
            "isCheckable": self.is_checkable,
            "checked": self.checked,
            "itemIcon": self.icon,
            "itemIconHover": self.icon_hover,
            "itemIconInactive": self.icon_inactive,
            "showCheckMark": self.show_check_mark,
        }
        if self.sub_menu:
            d["itemSubMenu"] = self.sub_menu.to_dict()
        else:
            d["itemSubMenu"] = None
        return d

    @classmethod
    def new(cls, name: str, action: Optional[Callable[[int], None]] = None,
            enable: bool = True) -> "MenuItem":
        """Go: NewMenuItem()"""
        item = cls()
        item.text = name
        item.is_active = enable
        item.action = action
        return item

MENU_ITEM_HINT_SHOW_ALL_WINDOWS = 1


class Menu:
    """Go: Menu struct"""
    def __init__(self):
        self.items: List[MenuItem] = []
        self.checkable_menu = False
        self.single_check = False
        self._item_count: int = 0

    def allocate_id(self) -> str:
        """Go: allocateId()"""
        id_str = str(self._item_count)
        self._item_count += 1
        return id_str

    def append_item(self, *items: MenuItem):
        """Go: AppendItem()"""
        for item in items:
            if item.text:
                item.id = self.allocate_id()
                self.items.append(item)

    def handle_action(self, item_id: str, timestamp: int) -> bool:
        """Go: HandleAction()"""
        for item in self.items:
            if item.id == item_id:
                if item.action:
                    item.action(timestamp)
                return True
        return False

    def generate_json(self) -> str:
        """Go: GenerateJSON()"""
        d = self.to_dict()
        return json.dumps(d)

    def to_dict(self) -> dict:
        return {
            "checkableMenu": self.checkable_menu,
            "singleCheck": self.single_check,
            "items": [item.to_dict() for item in self.items],
        }

    @staticmethod
    def new() -> "Menu":
        """Go: NewMenu()"""
        return Menu()
