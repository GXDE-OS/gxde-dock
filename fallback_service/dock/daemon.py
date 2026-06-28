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
Translation of daemon.go to Python 3 that is asapted to Wayland.

Lifecycle mgr.
"""

import logging

log = logging.getLogger("dock.Daemon")

dock_manager = None
module_ready = False


def start():
    global dock_manager, module_ready
    if dock_manager is not None:
        return True

    from .toplevel import is_wayland, init_display
    if not is_wayland():
        log.error("Rejected to launch. Service requires Wayland.")
        return False

    try:
        init_display()
    except Exception as e:
        log.error(f"Wayland failed: {e}")
        return False

    module_ready = True
    log.info("Dock daemon fallback service started")
    return True


def stop():
    global dock_manager, module_ready
    dock_manager = None
    module_ready = False
    log.info("Dock daemon fallback service stopped")


def get_dependencies():
    return []
