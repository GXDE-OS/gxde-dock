/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "showdesktopitem.h"
#include "constants.h"

#include <QLayout>
#include <QProcess>
#include <QPainter>
#include <QMouseEvent>

ShowDesktopItem::ShowDesktopItem(QWidget *parent)
    : QLabel(parent),
      m_isHovered(false),
      m_isPressed(false)
{
}

ShowDesktopItem::~ShowDesktopItem()
{
}

void ShowDesktopItem::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    update();

    QLabel::enterEvent(event);
}

void ShowDesktopItem::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    update();

    QLabel::leaveEvent(event);
}

void ShowDesktopItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return QLabel::mousePressEvent(event);
    }

    m_isPressed = true;
    update();

    QProcess::startDetached("/usr/lib/deepin-daemon/desktop-toggle");
}

void ShowDesktopItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return QLabel::mouseReleaseEvent(event);
    }

    m_isPressed = false;
    update();
}

void ShowDesktopItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    QRect destRect = rect();

    if (width() < height()) {
        destRect = destRect.marginsRemoved(QMargins(0, 1, 0, 1));
    } else {
        destRect = destRect.marginsRemoved(QMargins(1, 0, 1, 0));
    }

    if (Dock::isV23Mode()) {
        paintV23(painter);
        return;
    }

    if (m_isPressed) {
        painter.fillRect(destRect, "#2ca7f8");
    } else if(m_isHovered){
        painter.fillRect(destRect, QColor(255, 255, 255, 51));
    } else {
        painter.fillRect(destRect, QColor(255, 255, 255, 26));
    }
}

void ShowDesktopItem::paintV23(QPainter &painter)
{
    const bool light = Dock::isV23Light();

    QColor fill;
    if (m_isPressed)
        fill = light ? QColor(0, 0, 0, 38) : QColor(255, 255, 255, 64);
    else if (m_isHovered)
        fill = light ? QColor(0, 0, 0, 20) : QColor(255, 255, 255, 38);
    else
        fill = light ? QColor(255, 255, 255, 64) : QColor(255, 255, 255, 13);
    painter.fillRect(rect(), fill);

    QRect edge = rect();
    if (width() < height())
        edge.setRight(edge.left());
    else
        edge.setBottom(edge.top());
    painter.fillRect(edge, light ? QColor(0, 0, 0, 26) : QColor(255, 255, 255, 26));
}
