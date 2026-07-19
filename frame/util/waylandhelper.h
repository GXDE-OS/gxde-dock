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
 * ----------------------------------------------------------------------------
 * Generic helpers for Wayland session stores here. I am too lazy to write a
 * .cpp file so here is a inline header.
 */

#ifndef FRAME_UTIL_WAYLANDHELPER_H_
#define FRAME_UTIL_WAYLANDHELPER_H_

#include <QGuiApplication>
#include <QString>

namespace Wayland {

inline bool isWaylandSession() {
    if (!qGuiApp) {
        return false;
    }

    return QGuiApplication::platformName().contains(
        QStringLiteral("wayland"), Qt::CaseInsensitive);
}

}  // namespace Wayland

#endif  // FRAME_UTIL_WAYLANDHELPER_H_
