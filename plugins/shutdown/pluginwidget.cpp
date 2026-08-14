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

#include "pluginwidget.h"

#include <QSvgRenderer>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QIcon>

PluginWidget::PluginWidget(QWidget *parent)
    : QWidget(parent)
{
}

QSize PluginWidget::sizeHint() const
{
    return QSize(26, 26);
}

void PluginWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPixmap pixmap;
    QString iconName = "system-shutdown";
    int iconSize;
    const Dock::DisplayMode displayMode = qApp->property(PROP_DISPLAY_MODE).value<Dock::DisplayMode>();

    if (displayMode == Dock::Efficient) {
        iconName = iconName + "-symbolic";
        iconSize = 16;
    } else {
        iconSize = std::min(width(), height()) * 0.8;
    }

    pixmap = loadSvg(iconName, QSize(iconSize, iconSize));

    QPainter painter(this);
    // 用 pixmap 自身的 devicePixelRatio 计算居中偏移，而非控件的 devicePixelRatioF()：
    // Wayland HDPI 下两者在启动阶段会不一致(控件 1.25 vs pixmap 烘焙时的 2.0)，
    // 混用会导致图标整体偏向左上角。
    painter.drawPixmap(rect().center() - pixmap.rect().center() / pixmap.devicePixelRatioF(), pixmap);
}

const QPixmap PluginWidget::loadSvg(const QString &fileName, const QSize &size) const
{
    // 按当前 DPR 请求 pixmap，HDPI 下图标才不会糊
    return QIcon::fromTheme(fileName).pixmap(size, devicePixelRatioF());
}
