// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23QUICKSTYLE_H
#define V23QUICKSTYLE_H

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QString>

// Colors, fonts and icons shared by the V23 quick settings widgets
namespace V23Quick {

// The popup is light only for the V23 light theme on Wayland; otherwise
// DockPopupWindow keeps its dark classic background.
bool lightPopup();
QColor popupTextColor();
QColor dockTextColor();
QColor highlightColor();
QColor withAlpha(QColor color, qreal alpha);
QFont pixelFont(int pixelSize, QFont::Weight weight = QFont::Normal);

enum IconMode {
    Mask,       // a symbolic icon, painted all in the given color
    KeepColors, // only its gray parts take the color, e.g. not a red low battery
};

// An icon of icons/ in the plugin resources, by name without extension
QPixmap icon(const QString &name, const QSize &size, const QColor &color,
             qreal devicePixelRatio, IconMode mode = Mask);

void runCommand(const QString &command);

}

#endif // V23QUICKSTYLE_H
