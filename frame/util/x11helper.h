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
 * Sadly, QX11Info is NOT avaliable in Qt6, we're switching to
 * QNativeInterface::QX11Application here.
 * For I'm lazy today:
 *   1. We only provide minimal set of functions needed here.
 *   2. I just wrote a inline header rather than a full class.
 * ----------------------------------------------------------------------------
 * QX11Info::display()       -> x11Display()
 * QX11Info::connection()    -> x11Connection()
 * QX11Info::appRootWindow() -> DefaultRootWindow, which is x11Display()
 * QX11Info::getTimestamp()  -> Use your own timestamp instead.
 */

#ifndef GXDE_DOCK_X11HELPER_H
#define GXDE_DOCK_X11HELPER_H

#include <QGuiApplication>

// Xlib will define None as 0L, which CONFLICTS with QUrl::None
// That fails the complication of qurl.h:
//   error: expected identifier before numeric constant
//          None = 0x0,
// So just forward-declearing here.

typedef struct _XDisplay Display;
typedef struct xcb_connection_t xcb_connection_t;

/**
 * @brief Simple replacement for QX11Info::display().
 * @return (Xlib Display*) The display, or @c nullptr in Wayland.
 */
inline Display* x11Display() {
    auto* x11App = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11App) {
        qWarning() << "x11Display(): no X11 connection (Wayland?), returning nullptr";
        return nullptr;
    }
    return x11App->display();
}

/**
 * @brief Simple replacement for QX11Info::connection()
 * @return (xcb_connection_t*) The connection, or @c nullptr in Wayland.
 */
inline xcb_connection_t* x11Connection() {
    auto* x11App = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11App) {
        qWarning() << "x11Connection(): no X11 connection (Wayland?), returning nullptr";
        return nullptr;
    }
    return x11App->connection();
}

#endif // GXDE_DOCK_X11HELPER_H
