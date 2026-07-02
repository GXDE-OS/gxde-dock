/*
 * Copyright (C) 2026 CharOfString <markus_verify@126.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FRAME_WAYLAND_LAYERSHELLHELPER_H_
#define FRAME_WAYLAND_LAYERSHELLHELPER_H_

#include <QString>
#include <QWidget>
#include <QScreen>
#include <constants.h>

namespace Wayland {

class LayerShellHelper {
public:
    static bool isWayland();
    static bool isTreeland();
    static void setDockRole(QWidget* widget, QScreen* screen,
        const QString& scope, Dock::Position position);
    static void updateDockAnchor(QWidget* widget, Dock::Position position);
    static void updateExclusiveZone(QWidget* widget, int zone);
    static void updateOutput(QWidget* widget, QScreen* screen);
    static void fixPopupLayerShell(QWidget* popup);
    static void fixDockPopupLayerShell(QWidget* popup);
    static void styleDockPopupLayerShell(QWidget* popup);
    static void setMenuMaskRole(QWidget* widget);
};

}  // namespace Wayland

#endif  // FRAME_WAYLAND_LAYERSHELLHELPER_H_
