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

#include "hoverhighlighteffect.h"
#include "util/imagefactory.h"
#include "constants.h"

#include <QPainter>
#include <QDebug>

HoverHighlightEffect::HoverHighlightEffect(QObject *parent)
    : QGraphicsEffect(parent)

    , m_highlighting(false)
    , m_tintMonochrome(false)
{

}

// The dock's symbolic icons and plugin texts are drawn white for a dark
// panel, while a few tray icons are drawn black.  In V23 mode monochrome
// content is recolored to contrast with the panel theme; images containing
// real colors (application tray icons) are left untouched.  Pixels below the
// alpha threshold, such as the item hover plate, are not considered.
static QPixmap tintMonochrome(const QPixmap &pix, const bool toDark)
{
    QImage image = pix.toImage().convertToFormat(QImage::Format_ARGB32);

    int lightPixels = 0;
    int darkPixels = 0;
    int colorPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) < 72)
                continue;
            const QColor color(line[x]);
            if (color.hslSaturation() > 64 && color.lightness() > 30 && color.lightness() < 225)
                ++colorPixels;
            else if (color.lightness() > 150)
                ++lightPixels;
            else if (color.lightness() < 100)
                ++darkPixels;
        }
    }

    const int matched = toDark ? lightPixels : darkPixels;
    const int opposite = toDark ? darkPixels : lightPixels;
    if (!matched || colorPixels * 5 > matched || opposite > matched)
        return pix;

    const QRgb target = toDark ? qRgb(0x1f, 0x1f, 0x1f) : qRgb(0xf5, 0xf5, 0xf5);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (!alpha)
                continue;
            const QColor color(line[x]);
            if (color.hslSaturation() > 64)
                continue;
            if (toDark ? color.lightness() >= 100 : color.lightness() <= 150)
                line[x] = (target & 0x00ffffff) | (alpha << 24);
        }
    }

    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(pix.devicePixelRatio());
    return result;
}

void HoverHighlightEffect::draw(QPainter *painter)
{
    QPixmap pix = sourcePixmap(Qt::DeviceCoordinates);

    if (Dock::isV23Mode()) {
        // V23 shows hover with item plates instead of brightening the icon
        if (m_tintMonochrome)
            pix = tintMonochrome(pix, Dock::isV23Light());
        painter->drawPixmap(0, 0, pix);
        return;
    }

    if (m_highlighting)
    {
        painter->drawPixmap(0, 0, ImageFactory::lighterEffect(pix));
    } else {
        painter->drawPixmap(0, 0, pix);
    }
}
