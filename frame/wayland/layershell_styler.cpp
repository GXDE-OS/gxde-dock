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

void apply(QWindow *window, int radius, bool enableBlur) {
    if (!window || radius < 0) {
        return;
    }

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    auto *display = static_cast<wl_display *>(
        native->nativeResourceForWindow("display", nullptr));
    auto *surface = static_cast<wl_surface *>(
        native->nativeResourceForWindow("surface", window));

    if (!display || !surface) {
        return;
    }

    // Bind dde_shell and blur_manager
    wl_registry *registry = wl_display_get_registry(display);
    BindContext ctx;
    ctx.display = display;
    wl_registry_add_listener(registry, &registry_listener, &ctx);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    // Apply window radius via dde_shell
    if (radius > 0 && ctx.ddeShell) {
        dde_shell_surface *shellSurface =
            dde_shell_get_shell_surface(ctx.ddeShell, surface);

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

        // Also request no title bar
        wl_array emptyArr;
        wl_array_init(&emptyArr);
        dde_shell_surface_set_property(
            shellSurface,
            DDE_SHELL_PROPERTY_NOTITLEBAR,
            &emptyArr);
        wl_array_release(&emptyArr);

        wl_display_flush(display);
    }

    // Apply background blur via org_kde_kwin_blur
    if (enableBlur && ctx.blurManager) {
        org_kde_kwin_blur *blur =
            org_kde_kwin_blur_manager_create(ctx.blurManager, surface);

        // Blur the entire surface — pass nullptr region = whole window
        org_kde_kwin_blur_set_region(blur, nullptr);
        org_kde_kwin_blur_commit(blur);
        wl_display_flush(display);

        org_kde_kwin_blur_destroy(blur);
    }

    // Mark the window so MainPanel knows blur is active
    window->setProperty("_d_wayland_has_blur", enableBlur && ctx.blurManager);

    // Cleanup
    if (ctx.ddeShell) {
        dde_shell_destroy(ctx.ddeShell);
    }

    if (ctx.blurManager) {
        org_kde_kwin_blur_manager_destroy(ctx.blurManager);
    }
}

}  // namespace LayerShellStyler
}  // namespace Wayland
