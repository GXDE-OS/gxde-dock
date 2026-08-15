/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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

#include "abstracttraywidget.h"
#include "constants.h"

#include <xcb/xproto.h>
#include <QApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QDebug>
#include <QScreen>

namespace {

QPoint trayGlobalPoint(const QWidget *widget, const QPoint &localPoint)
{
    if (!widget)
        return QCursor::pos();

    if (!QGuiApplication::platformName().contains("wayland",
                                                  Qt::CaseInsensitive))
        return widget->mapToGlobal(localPoint);

    QWidget *topLevel = widget->window();
    QScreen *screen = topLevel ? topLevel->screen() : nullptr;
    if (!topLevel || !screen)
        return widget->mapToGlobal(localPoint);

    const QRect screenRect = screen->geometry();
    const QSize dockSize = topLevel->size();
    const Dock::Position position =
        qApp->property(PROP_POSITION).value<Dock::Position>();
    QPoint dockOrigin;

    switch (position) {
    case Dock::Top:
        dockOrigin = QPoint(screenRect.left()
                                + (screenRect.width() - dockSize.width()) / 2,
                            screenRect.top());
        break;
    case Dock::Bottom:
        dockOrigin = QPoint(screenRect.left()
                                + (screenRect.width() - dockSize.width()) / 2,
                            screenRect.bottom() - dockSize.height() + 1);
        break;
    case Dock::Left:
        dockOrigin = QPoint(screenRect.left(),
                            screenRect.top()
                                + (screenRect.height() - dockSize.height()) / 2);
        break;
    case Dock::Right:
        dockOrigin = QPoint(screenRect.right() - dockSize.width() + 1,
                            screenRect.top()
                                + (screenRect.height() - dockSize.height()) / 2);
        break;
    }

    return dockOrigin + widget->mapTo(topLevel, localPoint);
}

} // namespace

AbstractTrayWidget::AbstractTrayWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f),
    m_handleMouseReleaseTimer(new QTimer(this))
{
    m_handleMouseReleaseTimer->setSingleShot(true);
    m_handleMouseReleaseTimer->setInterval(100);

    connect(m_handleMouseReleaseTimer, &QTimer::timeout, this, &AbstractTrayWidget::handleMouseRelease);
}

AbstractTrayWidget::~AbstractTrayWidget()
{

}

void AbstractTrayWidget::mousePressEvent(QMouseEvent *event)
{
    // call QWidget::mousePressEvent means to show dock-context-menu
    // when right button of mouse is pressed immediately in fashion mode

    // here we hide the right button press event when it is click in the special area
    if (event->button() == Qt::RightButton && perfectIconRect().contains(event->pos())) {
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void AbstractTrayWidget::mouseReleaseEvent(QMouseEvent *e)
{
    //e->accept();

    // 由于 XWindowTrayWidget 中对 发送鼠标事件到X窗口的函数, 如 sendClick/sendHoverEvent 中
    // 使用了 setX11PassMouseEvent, 而每次调用 setX11PassMouseEvent 时都会导致产生 mousePress 和 mouseRelease 事件
    // 因此如果直接在这里处理事件会导致一些问题, 所以使用 Timer 来延迟处理 100 毫秒内的最后一个事件
    m_lastMouseReleaseData.first = e->pos();
    m_lastMouseReleaseData.second = e->button();

    m_handleMouseReleaseTimer->start();

    QWidget::mouseReleaseEvent(e);
}


void AbstractTrayWidget::handleMouseRelease() {

    Q_ASSERT(sender() == m_handleMouseReleaseTimer);

    // do not dealwith all mouse event of SystemTray, class SystemTrayItem will handle it
    if (trayTyep() == SystemTray)
        return;

    const QPoint point(m_lastMouseReleaseData.first - rect().center());
    if (point.manhattanLength() > 24)
        return;

    const QPoint globalPos = trayGlobalPoint(this,
                                              m_lastMouseReleaseData.first);
    uint8_t buttonIndex = XCB_BUTTON_INDEX_1;

    switch (m_lastMouseReleaseData.second) {
    case Qt:: MiddleButton:
        buttonIndex = XCB_BUTTON_INDEX_2;
        break;
    case Qt::RightButton:
        buttonIndex = XCB_BUTTON_INDEX_3;
        break;
    default:
        break;
    }

    sendClick(buttonIndex, globalPos.x(), globalPos.y());

    // left mouse button clicked
    if (buttonIndex == XCB_BUTTON_INDEX_1) {
        Q_EMIT clicked();
    }
}

const QRect AbstractTrayWidget::perfectIconRect() const
{
    const QRect itemRect = rect();
    const int iconSize = std::min(itemRect.width(), itemRect.height()) * 0.8;

    QRect iconRect;
    iconRect.setWidth(iconSize);
    iconRect.setHeight(iconSize);
    iconRect.moveTopLeft(itemRect.center() - iconRect.center());

    return iconRect;
}

QPoint AbstractTrayWidget::popupMenuPosition(const QSize &menuSize,
                                              const QWidget *menu) const
{
    QWidget *topLevel = window();
    QScreen *screen = topLevel ? topLevel->screen() : nullptr;
    if (!topLevel || !screen)
        return mapToGlobal(rect().center());

    const QRect screenRect = screen->geometry();
    const QSize dockSize = topLevel->size();
    const Dock::Position position =
        qApp->property(PROP_POSITION).value<Dock::Position>();
    const QMargins margins = menu ? menu->contentsMargins() : QMargins();
    const int panelWidth = menuSize.width() - margins.left() - margins.right();
    const int panelHeight = menuSize.height() - margins.top() - margins.bottom();
    QPoint dockOrigin;
    QPoint local = mapTo(topLevel, rect().center());

    switch (position) {
    case Dock::Top:
        dockOrigin = QPoint(screenRect.left()
                                + (screenRect.width() - dockSize.width()) / 2,
                            screenRect.top());
        local.setX(local.x() - margins.left() - panelWidth / 2);
        local.setY(dockSize.height() - margins.top());
        break;
    case Dock::Bottom:
        dockOrigin = QPoint(screenRect.left()
                                + (screenRect.width() - dockSize.width()) / 2,
                            screenRect.bottom() - dockSize.height() + 1);
        local.setX(local.x() - margins.left() - panelWidth / 2);
        local.setY(-menuSize.height() + margins.bottom());
        break;
    case Dock::Left:
        dockOrigin = QPoint(screenRect.left(),
                            screenRect.top()
                                + (screenRect.height() - dockSize.height()) / 2);
        local.setX(dockSize.width() - margins.left());
        local.setY(local.y() - margins.top() - panelHeight / 2);
        break;
    case Dock::Right:
        dockOrigin = QPoint(screenRect.right() - dockSize.width() + 1,
                            screenRect.top()
                                + (screenRect.height() - dockSize.height()) / 2);
        local.setX(-menuSize.width() + margins.right());
        local.setY(local.y() - margins.top() - panelHeight / 2);
        break;
    }

    const QRect screenLocal(screenRect.topLeft() - dockOrigin,
                            screenRect.size());
    local.setX(qBound(screenLocal.left(), local.x(),
                      screenLocal.right() - menuSize.width() + 1));
    local.setY(qBound(screenLocal.top(), local.y(),
                      screenLocal.bottom() - menuSize.height() + 1));
    return dockOrigin + local;
}
