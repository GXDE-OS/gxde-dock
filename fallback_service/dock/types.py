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
Translation of types.go to Python 3.

Enums: HideMode, DisplayMode, Position, HideState
Struct: Rect
"""

from enum import IntEnum


class HideMode(IntEnum):
    """Go: HideModeType"""
    KeepShowing = 0
    KeepHidden = 1
    AutoHide = 2    # Depreciated
    SmartHide = 3


class HideState(IntEnum):
    """Go: HideStateType"""
    Unknown = 0
    Show = 1
    Hide = 2


class DisplayMode(IntEnum):
    """Go: DisplayModeType"""
    Fashion = 0
    Efficient = 1
    Classic = 2    # Depreciated


class Position(IntEnum):
    """Go: positionType"""
    Top = 0
    Right = 1
    Bottom = 2
    Left = 3


class Rect:
    """Go: Rect struct"""
    __slots__ = ("x", "y", "width", "height")

    def __init__(self, x: int = 0, y: int = 0, width: int = 0, height: int = 0):
        self.x = x
        self.y = y
        self.width = width
        self.height = height

    def pieces(self) -> tuple:
        """Go: Pieces()"""
        return (self.x, self.y, self.width, self.height)

    def __repr__(self):
        return f"Rect(x = {self.x}, y = {self.y}, w = {self.width}, h = {self.height})"
