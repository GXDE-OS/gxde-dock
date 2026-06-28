#!/usr/bin/env python3

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
This is an unstable temporary fallback service for Wayland session.
Please do read the README.md for more information, and please do write a service
in the new gxde-daemon to replace this one in the future.

NOTE: We do use AI to accelerate the translation progress, and I have tested on my own
      VM. I suggest that to do a second-round review + testing when you are inspecting the
      merge request.

      This service is "harmless", it is designed to occupy the D-Bus name
      "top.gxde.dock.fallback.service.Main", which is not occupied by anything else by the system
      packages. The dock prefers the original D-Bus from dde-daemon, NOT this one. The dock ONLY
      use this service when the original D-Bus is NOT available.

      DO REMEMBER: THIS IS ONLY A TEMPORARY SOLUTION TO MAKE THE DEVELOPER'S LIFE EASIER SO THAT
                   THEY CAN FINISH THE DOCK/APP LAUNCHER/TOP BAR/DESKTOP PANEL FIRST. BE SURE TO
                   REPLACE THIS SERVICE WITH A MORE STABLE SOLUTION IN FUTURE.
"""

from . import app_entries, app_entry_interface, app_entry_menu, app_entry, app_info
from . import daemon, dock_manager
from . import identify_window
from . import menu
from . import plugin_settings
from . import sync_config
from . import toplevel, types
from . import util
from . import window
