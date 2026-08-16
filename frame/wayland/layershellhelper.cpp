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
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QScreen>
#include <QSet>
#include <QStyle>
#include <QWidget>
#include <QWindow>

#include <LayerShellQt/Window>
#include <dplatformwindowhandle.h>

#include "../util/dockpopupwindow.h"
#include "../util/waylandhelper.h"
#include "layershellhelper.h"
#include "layershell_styler.h"

DWIDGET_USE_NAMESPACE

namespace Wayland {

namespace {

QRect dockPopupPanelRect(DockPopupWindow *popup)
{
    if (!popup) {
        return QRect();
    }

    const QRect widgetRect = popup->rect();
    if (widgetRect.isEmpty()) {
        return QRect();
    }

    // DArrowRectangle paints its rounded body inside the shadow margins.
    // The arrow triangle protrudes from one edge; exclude both so the
    // compositor blur is clipped to the actual visible panel instead of
    // spilling into the transparent margins around it.
    const int shadow = qRound(popup->shadowBlurRadius());
    const int arrowHeight = popup->arrowHeight();
    QRect panel = widgetRect;
    switch (popup->arrowDirection()) {
    case DockPopupWindow::ArrowLeft:
        panel.adjust(shadow + arrowHeight, shadow, -shadow, -shadow);
        break;
    case DockPopupWindow::ArrowRight:
        panel.adjust(shadow, shadow, -(shadow + arrowHeight), -shadow);
        break;
    case DockPopupWindow::ArrowTop:
        panel.adjust(shadow, arrowHeight, -shadow, -shadow);
        break;
    case DockPopupWindow::ArrowBottom:
        panel.adjust(shadow, shadow, -shadow, -(shadow + arrowHeight));
        break;
    }

    return panel;
}

QSet<QWidget *> &preparedPopups()
{
    static QSet<QWidget *> popups;
    return popups;
}

void registerPreparedPopup(QWidget *popup)
{
    if (!popup || preparedPopups().contains(popup)) {
        return;
    }

    preparedPopups().insert(popup);
    QObject::connect(popup, &QObject::destroyed, popup, [popup] {
        preparedPopups().remove(popup);
    });
}

void closeOtherPreparedPopups(QWidget *popup)
{
    if (!popup) {
        return;
    }

    const QSet<QWidget *> openPopups = preparedPopups();
    for (QWidget *other : openPopups) {
        if (other == popup) {
            continue;
        }

        if (QMenu *menu = qobject_cast<QMenu *>(other)) {
            menu->close();
        } else {
            other->close();
        }
    }
}

void positionPreparedPopup(QWidget* popup, const QPoint& globalPosition)
{
    if (!popup || !popup->windowHandle())
        return;

    QScreen* screen = popup->windowHandle()->screen();
    if (!screen)
        screen = qApp->screenAt(globalPosition);
    if (!screen)
        screen = qApp->primaryScreen();

    const QPoint local = screen
        ? globalPosition - screen->geometry().topLeft()
        : globalPosition;
    if (LayerShellQt::Window* layer =
            LayerShellQt::Window::get(popup->windowHandle())) {
        layer->setMargins(QMargins(local.x(), local.y(), 0, 0));
        // A layer surface has no meaningful QWidget global position.  Keep
        // the requested position so nested menus can be placed relative to
        // their parent without reading compositor configure feedback.
        popup->setProperty("_d_layer_popup_global_position", globalPosition);
    }
}

class PreparedSubmenuPositioner final : public QObject {
public:
    explicit PreparedSubmenuPositioner(QMenu* submenu)
        : QObject(submenu)
        , m_submenu(submenu)
    {
        setObjectName(QStringLiteral("_d_layer_submenu_positioner"));
        connect(submenu, &QMenu::aboutToShow, this, [this] {
            positionSubmenu();
        });
    }

    void setAnchor(QMenu* parentMenu, QAction* parentAction)
    {
        m_parentMenu = parentMenu;
        m_parentAction = parentAction;
    }

private:
    void positionSubmenu()
    {
        if (!m_submenu || !m_parentMenu || !m_parentAction)
            return;

        const QVariant parentPosition = m_parentMenu->property(
            "_d_layer_popup_global_position");
        if (!parentPosition.isValid())
            return;

        m_parentMenu->ensurePolished();
        m_submenu->ensurePolished();
        const QSize submenuSize = m_submenu->sizeHint().expandedTo(QSize(1, 1));
        m_submenu->resize(submenuSize);

        const QRect actionRect = m_parentMenu->actionGeometry(m_parentAction);
        if (!actionRect.isValid())
            return;

        const QPoint parentGlobal = parentPosition.toPoint();
        int firstActionTop = 0;
        if (!m_submenu->actions().isEmpty()) {
            firstActionTop = m_submenu->actionGeometry(
                m_submenu->actions().constFirst()).top();
        }

        const QMargins parentMargins = m_parentMenu->contentsMargins();
        const QMargins submenuMargins = m_submenu->contentsMargins();
        QPoint requested;
        requested.setY(parentGlobal.y() + actionRect.top() - firstActionTop);

        const bool leftToRight =
            m_parentMenu->layoutDirection() == Qt::LeftToRight;
        const int parentPanelLeft = parentMargins.left();
        const int parentPanelRight = m_parentMenu->width()
            - parentMargins.right();
        if (leftToRight) {
            // 子菜单可见面板的左边缘贴住母菜单可见面板的右边缘，不留空隙。
            requested.setX(parentGlobal.x() + parentPanelRight
                           - submenuMargins.left());
        } else {
            // 子菜单可见面板的右边缘贴住母菜单可见面板的左边缘，不留空隙。
            requested.setX(parentGlobal.x() + parentPanelLeft
                           - submenuSize.width() + submenuMargins.right());
        }

        QScreen* screen = m_parentMenu->windowHandle()
            ? m_parentMenu->windowHandle()->screen() : nullptr;
        if (!screen && m_submenu->windowHandle())
            screen = m_submenu->windowHandle()->screen();
        if (!screen)
            screen = qApp->screenAt(parentGlobal);
        if (!screen)
            screen = qApp->primaryScreen();

        if (screen) {
            const QRect available = screen->geometry();
            if (leftToRight
                    && requested.x() + submenuSize.width() > available.right() + 1) {
                requested.setX(parentGlobal.x() + parentPanelLeft
                               - submenuSize.width() + submenuMargins.right());
            } else if (!leftToRight && requested.x() < available.left()) {
                requested.setX(parentGlobal.x() + parentPanelRight
                               - submenuMargins.left());
            }

            const int maxX = qMax(available.left(),
                                  available.right() - submenuSize.width() + 1);
            const int maxY = qMax(available.top(),
                                  available.bottom() - submenuSize.height() + 1);
            requested.setX(qBound(available.left(), requested.x(), maxX));
            requested.setY(qBound(available.top(), requested.y(), maxY));
        }

        positionPreparedPopup(m_submenu, requested);
    }

    QPointer<QMenu> m_submenu;
    QPointer<QMenu> m_parentMenu;
    QPointer<QAction> m_parentAction;
};

} // namespace

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

static LayerShellQt::Window::Anchor exclusiveEdgeForPosition(
        Dock::Position position) {
    switch (position) {
        case Dock::Top: {
            return LayerShellQt::Window::AnchorTop;
        }

        case Dock::Bottom: {
            return LayerShellQt::Window::AnchorBottom;
        }

        case Dock::Left: {
            return LayerShellQt::Window::AnchorLeft;
        }

        case Dock::Right: {
            return LayerShellQt::Window::AnchorRight;
        }
    }

    return LayerShellQt::Window::AnchorBottom;
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
    return isWaylandSession();
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

    if (screen) {
        widget->setScreen(screen);
    }

    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (!layer) {
        qWarning() << "(LayerShellHelper) failed to get layer window, halted!";
        return;
    }

    QWindow* window = widget->windowHandle();
    layer->setScope(scope);
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setAnchors(anchorsForPosition(position));
    layer->setExclusiveEdge(exclusiveEdgeForPosition(position));
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);

    // Treeland上为layer-shell禁用标题栏
    if (window) {
        DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
    }

    // Apply rounded corners and blur via dde_shell / org_kde_kwin_blur
    LayerShellStyler::apply(window, 5, true);
}

void LayerShellHelper::updateDockAnchor(QWidget* widget,
        Dock::Position position) {
    if (widget == nullptr || !isWayland()) {
        return;
    }

    LayerShellQt::Window* layer = layerWindowFor(widget);
    if (layer) {
        layer->setAnchors(anchorsForPosition(position));
        layer->setExclusiveEdge(exclusiveEdgeForPosition(position));
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

void LayerShellHelper::preparePopupLayerShell(QWidget* popup, QScreen* screen,
        const QPoint& globalPosition, bool allowKeyboardFocus,
        bool closeOtherPopups, QMenu* parentMenuOverride) {
    if (!popup || !isWayland())
        return;

    if (closeOtherPopups) {
        closeOtherPreparedPopups(popup);
    }

    QMenu* menuWidget = qobject_cast<QMenu*>(popup);
    QMenu* parentMenu = parentMenuOverride;
    if (!parentMenu && menuWidget && menuWidget->parentWidget()) {
        parentMenu = qobject_cast<QMenu*>(menuWidget->parentWidget());
    }
    const bool isSubMenu = parentMenu != nullptr;

    popup->ensurePolished();
    popup->resize(popup->sizeHint());

    const auto menuPanelRect = [](QMenu* menu) {
        const QMargins margins = menu->contentsMargins();
        return QRect(QPoint(margins.left(), margins.top()),
                     QSize(menu->width() - margins.left() - margins.right(),
                           menu->height() - margins.top() - margins.bottom()));
    };

    if (!popup->property("_d_layer_popup_prepared").toBool()) {
        // 根菜单仍然使用 Qt::Tool，避免 QtWayland 把无 transient parent 的
        // Qt::Popup 猜成最后输入的 Dock 窗口，再被 Treeland 配满整个屏幕。
        // 子菜单必须保留 QMenu 的 Qt::Popup 类型和 transient parent，这样
        // QApplication 才能把它纳入 popup 链；否则鼠标进入子菜单时，高亮会
        // 弹回母菜单。子菜单依旧是 layer surface，位置由
        // PreparedSubmenuPositioner 在 aboutToShow 时手动摆放。
        Qt::WindowFlags flags = popup->windowFlags();
        flags &= ~Qt::WindowFlags(Qt::WindowType_Mask);
        flags |= (isSubMenu ? Qt::Popup : Qt::Tool) | Qt::FramelessWindowHint;
        popup->setWindowFlags(flags);
        if (!isSubMenu)
            popup->move(globalPosition);
        popup->setAttribute(Qt::WA_NativeWindow, true);

        QWindow* window = popup->windowHandle();
        if (!window) {
            qWarning() << "(LayerShellHelper) invalid prepared popup handle";
            return;
        }
        if (screen)
            window->setScreen(screen);
        if (isSubMenu) {
            if (parentMenu->windowHandle())
                window->setTransientParent(parentMenu->windowHandle());
        } else {
            window->setTransientParent(nullptr);
        }

        // Window::get creates the native surface immediately in
        // LayerShellQt 6.3.  The window type, screen and size above therefore
        // have to be set before this call.
        LayerShellQt::Window* layer = LayerShellQt::Window::get(window);
        if (!layer)
            return;

        LayerShellQt::Window::Anchors anchors;
        anchors |= LayerShellQt::Window::AnchorTop;
        anchors |= LayerShellQt::Window::AnchorLeft;
        layer->setAnchors(anchors);
        layer->setScreenConfiguration(
            LayerShellQt::Window::ScreenFromQWindow);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(
            allowKeyboardFocus && !isSubMenu
                ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                : LayerShellQt::Window::KeyboardInteractivityNone);
        DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
        LayerShellStyler::apply(window, 8, true,
                                menuWidget ? menuPanelRect(menuWidget)
                                           : QRect());

        popup->setProperty("_d_layer_popup_prepared", true);
    }

    registerPreparedPopup(popup);
    positionPreparedPopup(popup, globalPosition);

    // 子菜单仍然是独立的 layer surface。提前创建它们，并在 aboutToShow
    // 时用保存的父菜单位置和 actionGeometry 手动计算 margin。
    if (menuWidget) {
        for (QAction* action : menuWidget->actions()) {
            if (QMenu* submenu = action->menu()) {
                preparePopupLayerShell(submenu, screen, globalPosition,
                                       false, false, menuWidget);
                QObject* object = submenu->findChild<QObject*>(
                    QStringLiteral("_d_layer_submenu_positioner"),
                    Qt::FindDirectChildrenOnly);
                auto* positioner = static_cast<PreparedSubmenuPositioner*>(object);
                if (!positioner)
                    positioner = new PreparedSubmenuPositioner(submenu);
                positioner->setAnchor(menuWidget, action);
            }
        }
    }
}

static QWindow* configurePopupLayerShell(QWidget* popup,
        bool allowKeyboardFocus) {
    if (popup == nullptr) {
        qWarning() << "(LayerShellHelper) configurePopupLayerShell got a null popup!";
        return nullptr;
    }

    if (!LayerShellHelper::isWayland()) {
        return nullptr;
    }

    popup->createWinId();

    QWindow* window = popup->windowHandle();
    if (!window) {
        qWarning() << "(LayerShellHelper) invalid popup window handle, halted!";
        return nullptr;
    }

    // 不设anchor的话Treeland会把弹窗摆到屏幕中间，有时候还会糊屏幕上
    // 改为锚定左上角，再用margin偏移到弹出位置 (popup->pos())
    const QVariant requestedPosition =
        popup->property("_d_dock_popup_position");
    const QPoint pos = requestedPosition.isValid()
        ? requestedPosition.toPoint()
        : popup->pos();
    // QMenu is created without a parent window, so on a multi-output
    // Wayland session Qt initially assigns it to the primary screen even
    // when exec()/popup() was given a point on another output.  Select the
    // output from that requested global point before configuring layer-shell.
    QScreen* screen = qApp->screenAt(pos);
    if (!screen)
        screen = window->screen();
    if (screen && window->screen() != screen)
        window->setScreen(screen);

    LayerShellQt::Window* layer = LayerShellQt::Window::get(window);
    if (!layer) {
        return nullptr;
    }

    const QPoint localPos = screen
        ? pos - screen->geometry().topLeft()
        : pos;
    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    layer->setAnchors(anchors);
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    layer->setMargins(QMargins(localPos.x(), localPos.y(), 0, 0));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setExclusiveZone(0);

    // 子菜单 (如「位置/大小」展开项) 不要键盘交互
    // 否则它会requestActive抢走激活态，Treeland 把父菜单设为非激活
    // 然后整个菜单就被关了
    const bool acceptsKeyboard = allowKeyboardFocus
        && window->transientParent() == nullptr;
    layer->setKeyboardInteractivity(
        acceptsKeyboard ? LayerShellQt::Window::KeyboardInteractivityOnDemand
            : LayerShellQt::Window::KeyboardInteractivityNone);

    DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);

    return window;
}

void LayerShellHelper::fixPopupLayerShell(QWidget* popup) {
    configurePopupLayerShell(popup, true);
}

void LayerShellHelper::fixDockPopupLayerShell(QWidget* popup) {
    configurePopupLayerShell(popup, false);
}

void LayerShellHelper::styleDockPopupLayerShell(QWidget* popup) {
    if (!popup || !isWayland()) {
        return;
    }

    QWindow* window = popup->windowHandle();
    if (!window || window->property("_d_dock_popup_styled").toBool()) {
        return;
    }

    // Plugin popups are DArrowRectangle windows: the visible rounded panel is
    // smaller than the toplevel surface because the rectangle reserves room
    // for its shadow and arrow.  Blur only that panel, otherwise KWin blurs
    // the transparent shadow margins and the blur visibly overflows the popup.
    if (DockPopupWindow *dockPopup = qobject_cast<DockPopupWindow *>(popup)) {
        const QRect panel = dockPopupPanelRect(dockPopup);
        const bool panelValid = panel.isValid() && !panel.isEmpty();
        LayerShellStyler::apply(window, dockPopup->radius(), panelValid, panel);
    } else {
        LayerShellStyler::apply(window, 6, true);
    }
    window->setProperty("_d_dock_popup_styled", true);
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
    // exclusive zone 必须为 -1：Treeland/wlroots 对 exclusive_zone >= 0 的
    // layer surface 会避开 dock 的保留区域（validGeometry），导致 mask 盖不住
    // dock 自身，点击 dock 范围内、菜单范围外的位置时事件被 dock 收走，菜单无法关闭。
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);

    // Treeland: 需要"dde-shell/dock"，Treeland才不会为其添加边框
    layer->setScope(QStringLiteral("dde-shell/dock"));

    DPlatformWindowHandle::setEnableNoTitlebarForWindow(window, true);
}

}  // namespace Wayland
