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

#include <QDebug>
#include <QGuiApplication>
#include <QMargins>
#include <QPoint>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include <LayerShellQt/Window>
#include <dplatformwindowhandle.h>

#include "layershellhelper.h"

DWIDGET_USE_NAMESPACE

namespace Wayland {

static LayerShellQt::Window::Anchors anchorsForPosition(
        Dock::Position position) {
    switch (position) {
        case Dock::Top: {
            return LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorTop);
        }

        case Dock::Bottom: {
            return LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorBottom);
        }
        case Dock::Left: {
            return LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorLeft);
        }

        case Dock::Right: {
            return LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorRight);
        }
    }

    return LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom);
}

static LayerShellQt::Window* layerWindowFor(QWidget* widget) {
    widget->setAttribute(Qt::WA_NativeWindow, true);
    widget->createWinId();

    QWindow* window = widget->windowHandle();
    if (!window) {
        qWarning() << "(LayerShellHelper) invalid window handle: " << widget;
        return nullptr;
    }

    return LayerShellQt::Window::get(window);
}

bool LayerShellHelper::isWayland() {
    if (!qGuiApp) {
        return false;
    }

    return QGuiApplication::platformName().toLower().contains("wayland");
}

// Treeland会话检测
bool LayerShellHelper::isTreeland() {
    if (!isWayland()) {
        return false;
    }

    if (qEnvironmentVariable("XDG_SESSION_DESKTOP").toLower().contains(
                QLatin1String("treeland")) ||
            qEnvironmentVariable("DESKTOP_SESSION").toLower().contains(
               QLatin1String("treeland")) ||
            qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower().contains(
               QLatin1String("treeland")) ||
            qEnvironmentVariable("GDMSESSION").toLower().contains(
               QLatin1String("treeland"))) {
        return true;
    } else {
        return false;
    }
}

void LayerShellHelper::setDockRole(QWidget* widget, QScreen* screen,
        const QString& scope, Dock::Position position) {
    if (widget == nullptr) {
        qWarning() << "(LayerShellHelper) setDockRole got a null widget!";
        return;
    }

    if (!isWayland()) {
        return;
    }

    widget->setWindowFlag(Qt::FramelessWindowHint, true);

    QWindow* window = widget->windowHandle();
    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (!layer) {
        qWarning() << "(LayerShellHelper) failed to get layer window, halted!";
        return;
    }

    if (screen) {
        widget->windowHandle()->setScreen(screen);
    }

    layer->setScope(scope);
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setAnchors(anchorsForPosition(position));
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);

    // Treeland上为layer-shell禁用标题栏
    if (window) {
        DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
    }
}

void LayerShellHelper::updateDockAnchor(QWidget* widget,
        Dock::Position position) {
    if (widget == nullptr || !isWayland()) {
        return;
    }

    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (layer) {
        layer->setAnchors(anchorsForPosition(position));
    }
}

void LayerShellHelper::updateExclusiveZone(QWidget* widget, int zone) {
    if (widget == nullptr || !isWayland()) {
        return;
    }

    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (layer) {
        layer->setExclusiveZone(zone);
    }
}

void LayerShellHelper::updateOutput(QWidget* widget, QScreen* screen) {
    if (widget == nullptr || screen == nullptr || !isWayland()) {
        return;
    }

    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (layer) {
        widget->windowHandle()->setScreen(screen);
        layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    }
}

void LayerShellHelper::fixPopupLayerShell(QWidget* popup) {
    if (popup == nullptr) {
        qWarning() << "(LayerShellHelper) fixPopupLayerShell got a null popup!";
        return;
    }

    if (!isWayland()) {
        return;
    }

    popup->createWinId();

    QWindow* window = popup->windowHandle();
    if (!window) {
        qWarning() << "(LayerShellHelper) invalid popup window handle, halted!";
        return;
    }

    LayerShellQt::Window* layer = LayerShellQt::Window::get(window);
    if (!layer) {
        return;
    }

    // 不设anchor的话Treeland会把弹窗摆到屏幕中间，有时候还会糊屏幕上
    // 改为锚定左上角，再用margin偏移到弹出位置 (popup->pos())
    const QPoint pos = popup->pos();
    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    layer->setAnchors(anchors);
    layer->setMargins(QMargins(pos.x(), pos.y(), 0, 0));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setExclusiveZone(0);

    // 子菜单 (如「位置/大小」展开项) 不要键盘交互
    // 否则它会requestActive抢走激活态，Treeland 把父菜单设为非激活
    // 然后整个菜单就被关了
    const bool isSubMenu = window->transientParent() != nullptr;
    layer->setKeyboardInteractivity(
        isSubMenu ? LayerShellQt::Window::KeyboardInteractivityNone
            : LayerShellQt::Window::KeyboardInteractivityOnDemand);

    DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
}

// 全屏Mask的layer-shell属性
void LayerShellHelper::setMenuMaskRole(QWidget* widget) {
    if (widget == nullptr || !isWayland()) {
        return;
    }

    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setWindowFlags(Qt::FramelessWindowHint |
        Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    widget->createWinId();

    QWindow* window = widget->windowHandle();
    if (!window) {
        return;
    }

    LayerShellQt::Window* layer = LayerShellQt::Window::get(window);
    if (!layer) {
        return;
    }

    // 四边锚定: mask铺满屏幕
    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorBottom;
    anchors |= LayerShellQt::Window::AnchorLeft;
    anchors |= LayerShellQt::Window::AnchorRight;
    layer->setAnchors(anchors);
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setExclusiveZone(0);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);

    // Treeland: 需要"dde-shell/dock"，Treeland才不会为其添加边框
    layer->setScope(QStringLiteral("dde-shell/dock"));

    DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
}

}  // namespace Wayland
