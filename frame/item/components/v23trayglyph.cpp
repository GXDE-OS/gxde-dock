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

#include "v23trayglyph.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal kStroke = 1.33;

void setupBox(QPainter &painter, const QRectF &rect, qreal rotation) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.center());
    painter.rotate(rotation);
    painter.scale(rect.width() / 16, rect.height() / 16);
    painter.translate(-8, -8);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Dock::isV23Light() ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white),
        kStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

}  // namespace

namespace V23TrayGlyph {

void paintExpand(QPainter &painter, const QRectF &rect, bool horizontal, qreal rotation) {
    painter.save();
    setupBox(painter, rect, horizontal ? rotation : rotation + 90);
    painter.drawEllipse(QPointF(8, 8), 6.17, 6.17);

    QPainterPath path;
    path.moveTo(9.4, 4.7);
    path.lineTo(6.2, 8);
    path.lineTo(9.4, 11.3);
    painter.drawPath(path);

    painter.restore();
}

}  // namespace V23TrayGlyph
