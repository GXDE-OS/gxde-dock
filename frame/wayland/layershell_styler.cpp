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

#include <QWindow>
#include <QGuiApplication>
#include <QHash>
#include <QObject>
#include <QRegion>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#include "layershell_styler.h"
#include "protocols/dde-shell-client-protocol.h"
#include "protocols/blur-client-protocol.h"

namespace Wayland {
namespace LayerShellStyler {

struct BindContext {
    wl_display *display;
    dde_shell *ddeShell = nullptr;
    org_kde_kwin_blur_manager *blurManager = nullptr;
};

using ShellSurfaceMap = QHash<QWindow *, dde_shell_surface *>;
using BlurObjectMap = QHash<QWindow *, org_kde_kwin_blur *>;

ShellSurfaceMap &shellSurfaces()
{
    static ShellSurfaceMap surfaces;
    return surfaces;
}

BlurObjectMap &blurObjects()
{
    static BlurObjectMap objects;
    return objects;
}

QRegion roundedRegion(const QRect &rect, int radius)
{
    if (radius <= 0 || rect.isEmpty()) {
        return QRegion(rect);
    }

    QRegion region(rect.adjusted(radius, 0, -radius, 0));
    region += QRegion(rect.adjusted(0, radius, 0, -radius));

    const int diameter = radius * 2;
    region += QRegion(rect.x(), rect.y(), diameter, diameter, QRegion::Ellipse)
        & QRect(rect.x(), rect.y(), radius, radius);
    region += QRegion(rect.right() - diameter + 1, rect.y(), diameter,
                      diameter, QRegion::Ellipse)
        & QRect(rect.right() - radius + 1, rect.y(), radius, radius);
    region += QRegion(rect.x(), rect.bottom() - diameter + 1, diameter,
                      diameter, QRegion::Ellipse)
        & QRect(rect.x(), rect.bottom() - radius + 1, radius, radius);
    region += QRegion(rect.right() - diameter + 1,
                      rect.bottom() - diameter + 1, diameter, diameter,
                      QRegion::Ellipse)
        & QRect(rect.right() - radius + 1, rect.bottom() - radius + 1,
                radius, radius);
    return region;
}

void cleanupWindowResources(QWindow *window)
{
    if (!window) {
        return;
    }

    if (org_kde_kwin_blur *blur = blurObjects().take(window)) {
        org_kde_kwin_blur_release(blur);
    }

    if (dde_shell_surface *shellSurface = shellSurfaces().take(window)) {
        dde_shell_surface_destroy(shellSurface);
    }
}

void ensureWindowCleanup(QWindow *window)
{
    if (!window || window->property("_d_layershell_styler_cleanup").toBool()) {
        return;
    }

    window->setProperty("_d_layershell_styler_cleanup", true);
    QObject::connect(window, &QObject::destroyed, window, [window] {
        cleanupWindowResources(window);
    });
}

static void registry_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    auto *ctx = static_cast<BindContext *>(data);
    if (strcmp(interface, dde_shell_interface.name) == 0) {
        ctx->ddeShell = static_cast<dde_shell *>(
            wl_registry_bind(registry, name, &dde_shell_interface, 2));
    } else if (strcmp(interface, org_kde_kwin_blur_manager_interface.name) == 0) {
        ctx->blurManager = static_cast<org_kde_kwin_blur_manager *>(
            wl_registry_bind(registry, name, &org_kde_kwin_blur_manager_interface, 1));
    }
}

static void registry_global_remove(void *, wl_registry *, uint32_t) {}

static const wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

BindContext displayBindings(wl_display *display)
{
    static QHash<wl_display *, BindContext> bindings;
    const auto it = bindings.constFind(display);
    if (it != bindings.constEnd()) {
        return it.value();
    }

    BindContext ctx;
    ctx.display = display;
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &ctx);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    bindings.insert(display, ctx);
    return ctx;
}

void apply(QWindow *window, int radius, bool enableBlur, const QRect &blurRect) {
    if (!window || radius < 0) {
        return;
    }

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    auto *display = static_cast<wl_display *>(
        native->nativeResourceForIntegration("display"));
    if (!display) {
        display = static_cast<wl_display *>(
            native->nativeResourceForWindow("display", nullptr));
    }
    auto *surface = static_cast<wl_surface *>(
        native->nativeResourceForWindow("surface", window));
    auto *compositor = static_cast<wl_compositor *>(
        native->nativeResourceForIntegration("compositor"));

    if (!display || !surface) {
        return;
    }

    // Bind dde_shell and blur_manager once per wl_display.  Binding a new
    // registry and doing a synchronous roundtrip for every popup was the
    // main reason the dock settings menu took so long to appear.
    const BindContext ctx = displayBindings(display);

    ensureWindowCleanup(window);

    // Apply window radius via dde_shell
    if (radius > 0 && ctx.ddeShell) {
        dde_shell_surface *shellSurface = shellSurfaces().value(window);
        if (!shellSurface) {
            shellSurface = dde_shell_get_shell_surface(ctx.ddeShell, surface);
            if (shellSurface) {
                shellSurfaces().insert(window, shellSurface);
            }
        }

        if (shellSurface) {
            float vals[2] = { static_cast<float>(radius), static_cast<float>(radius) };
            wl_array dataArr;
            wl_array_init(&dataArr);
            float *arr_data = static_cast<float *>(
                wl_array_add(&dataArr, sizeof(float) * 2));
            arr_data[0] = vals[0];
            arr_data[1] = vals[1];
            dde_shell_surface_set_property(
                shellSurface,
                DDE_SHELL_PROPERTY_WINDOWRADIUS,
                &dataArr);
            wl_array_release(&dataArr);

            // Also request no title bar.  dde_shell expects one int here,
            // mirroring DTK's DDdeShellManager::setNoTitleBar().
            wl_array noTitleBarArr;
            wl_array_init(&noTitleBarArr);
            int *noTitleBarValue = static_cast<int *>(
                wl_array_add(&noTitleBarArr, sizeof(int)));
            if (noTitleBarValue) {
                *noTitleBarValue = 1;
            }
            dde_shell_surface_set_property(
                shellSurface,
                DDE_SHELL_PROPERTY_NOTITLEBAR,
                &noTitleBarArr);
            wl_array_release(&noTitleBarArr);

            wl_display_flush(display);
        }
    }

    // Apply background blur via org_kde_kwin_blur.  A non-empty blurRect
    // restricts the blur to the rounded panel area; using the whole surface
    // here would also blur the transparent shadow margins around a menu.
    bool blurApplied = false;
    if (enableBlur && ctx.blurManager) {
        if (org_kde_kwin_blur *oldBlur = blurObjects().take(window)) {
            org_kde_kwin_blur_release(oldBlur);
        }

        org_kde_kwin_blur *blur =
            org_kde_kwin_blur_manager_create(ctx.blurManager, surface);
        if (blur) {
            bool regionApplied = false;
            if (blurRect.isValid() && !blurRect.isEmpty()) {
                if (!compositor) {
                    // A shaped menu blur is impossible without a compositor.
                    org_kde_kwin_blur_release(blur);
                } else {
                    const int cornerRadius = qBound(
                        0, radius, qMin(blurRect.width(), blurRect.height()) / 2);
                    const QRegion region = roundedRegion(blurRect, cornerRadius);
                    wl_region *wlRegion =
                        wl_compositor_create_region(compositor);
                    for (const QRect &rect : region) {
                        wl_region_add(wlRegion, rect.x(), rect.y(),
                                      rect.width(), rect.height());
                    }
                    org_kde_kwin_blur_set_region(blur, wlRegion);
                    wl_region_destroy(wlRegion);
                    regionApplied = true;
                }
            } else {
                org_kde_kwin_blur_set_region(blur, nullptr);
                regionApplied = true;
            }

            if (regionApplied) {
                blurObjects().insert(window, blur);
                org_kde_kwin_blur_set_strength(blur, 300);
                org_kde_kwin_blur_commit(blur);
                blurApplied = true;
            }
        }
    } else {
        if (org_kde_kwin_blur *blur = blurObjects().take(window)) {
            org_kde_kwin_blur_release(blur);
        }
    }

    wl_display_flush(display);

    // Mark the window so MainPanel knows blur is active
    window->setProperty("_d_wayland_has_blur", blurApplied);

}

}  // namespace LayerShellStyler
}  // namespace Wayland
