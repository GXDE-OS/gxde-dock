/*
 * Copyright (C) 2026 GXDE OS
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
 */

#include "bluetoothicon.h"

#include <QSvgRenderer>
#include <QPainter>
#include <QImage>

QPixmap bluetoothIcon(const QString &name, int size, qreal devicePixelRatio,
                      const QColor &color)
{
    const QString file = QStringLiteral(":/icons/") + name + QStringLiteral(".svg");
    QSvgRenderer renderer(file);
    if (!renderer.isValid())
        return QPixmap();

    const QSize pixelSize(qRound(size * devicePixelRatio), qRound(size * devicePixelRatio));
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    renderer.render(&painter, QRectF(QPointF(0, 0), pixelSize));
    painter.end();

    // 重上色：保留 alpha，将 RGB 设为目标颜色（源图为白色 mask）
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0)
                continue;
            const int a = alpha * color.alpha() / 255;
            line[x] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), a));
        }
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}
