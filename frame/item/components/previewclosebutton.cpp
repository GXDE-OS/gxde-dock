/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
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

#include "previewclosebutton.h"
#include "constants.h"

#include <QPainter>

PreviewCloseButton::PreviewCloseButton(QWidget *parent)
    : QAbstractButton(parent)
    , m_hover(false)
{
    setFixedSize(24, 24);
    setCursor(Qt::ArrowCursor);
}

void PreviewCloseButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const bool light = Dock::isV23Light();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // circle: the palette's light/dark colors at 60% (stronger when hovered)
    QColor circle = light ? QColor(255, 255, 255) : QColor(70, 70, 70);
    circle.setAlphaF(isDown() ? 1.0 : m_hover ? 0.85 : 0.6);
    painter.setPen(Qt::NoPen);
    painter.setBrush(circle);
    painter.drawEllipse(QRectF(rect()).adjusted(.5, .5, -.5, -.5));

    // 16px "close" icon, its cross spans about half of it
    QPen pen(light ? QColor(0, 0, 0, 255 * 0.7) : QColor(255, 255, 255, 255 * 0.8), 1.5);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    const QPointF c = QRectF(rect()).center();
    const qreal d = 4;
    painter.drawLine(c + QPointF(-d, -d), c + QPointF(d, d));
    painter.drawLine(c + QPointF(d, -d), c + QPointF(-d, d));
}

void PreviewCloseButton::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();

    QAbstractButton::enterEvent(e);
}

void PreviewCloseButton::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();

    QAbstractButton::leaveEvent(e);
}
