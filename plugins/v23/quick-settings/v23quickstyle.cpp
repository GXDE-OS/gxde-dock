// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23quickstyle.h"
#include "constants.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include <QSvgRenderer>

namespace V23Quick {

bool lightPopup()
{
    return Dock::isV23Light();
}

QColor popupTextColor()
{
    return lightPopup() ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white);
}

QColor dockTextColor()
{
    return Dock::isV23Light() ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white);
}

QColor highlightColor()
{
    return qApp->palette().color(QPalette::Highlight);
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

QFont pixelFont(int pixelSize, QFont::Weight weight)
{
    QFont font = qApp->font();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

// The icons come at the sizes they were drawn for, e.g. "laptop_16.svg" and
// "laptop_24.svg"; take the one drawn for the asked size, or the closest one
static QString iconFile(const QString &name, int size)
{
    const QString base = QStringLiteral(":/v23-quick-settings/icons/") + name;
    QStringList candidates { QString("%1_%2").arg(base).arg(size), base };
    for (int drawn : {24, 20, 16}) {
        if (size <= drawn)
            candidates << QString("%1_%2").arg(base).arg(drawn);
    }
    for (int drawn : {16, 20, 24})
        candidates << QString("%1_%2").arg(base).arg(drawn);

    for (const QString &candidate : candidates) {
        for (const char *suffix : {".svg", ".png"}) {
            if (QFile::exists(candidate + suffix))
                return candidate + suffix;
        }
    }

    return QString();
}

static QImage loadIcon(const QString &name, const QSize &size, const QSize &pixelSize)
{
    const QString file = iconFile(name, size.width());
    if (file.endsWith(".svg")) {
        QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QSvgRenderer renderer(file);
        QPainter painter(&image);
        renderer.render(&painter, QRectF(QPointF(0, 0), pixelSize));
        return image;
    }

    QImage image(file);
    if (image.isNull())
        return image;

    return image.scaled(pixelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QPixmap icon(const QString &name, const QSize &size, const QColor &color, qreal devicePixelRatio, IconMode mode)
{
    static QHash<QString, QPixmap> cache;
    const QString cacheKey = QString("%1|%2x%3|%4|%5|%6").arg(name).arg(size.width()).arg(size.height())
        .arg(color.name(QColor::HexArgb)).arg(devicePixelRatio).arg(mode);
    auto it = cache.constFind(cacheKey);
    if (it != cache.constEnd())
        return it.value();

    QImage image = loadIcon(name, size, size * devicePixelRatio);
    if (image.isNull())
        return QPixmap();

    // The icons are drawn black on transparent, recolor them keeping the alpha
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0)
                continue;

            // KeepColors leaves the colored parts alone, e.g. a red low
            // battery, and recolors the gray ones, black or white
            const QRgb pixel = qUnpremultiply(line[x]);
            if (mode == KeepColors) {
                const int high = qMax(qMax(qRed(pixel), qGreen(pixel)), qBlue(pixel));
                const int low = qMin(qMin(qRed(pixel), qGreen(pixel)), qBlue(pixel));
                if (high - low > 0x30)
                    continue;
            }

            const int a = alpha * color.alpha() / 255;
            line[x] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), a));
        }
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    cache.insert(cacheKey, pixmap);
    return pixmap;
}

void runCommand(const QString &command)
{
    const QStringList args = QProcess::splitCommand(command);
    if (!args.isEmpty())
        QProcess::startDetached(args.first(), args.mid(1));
}

}
