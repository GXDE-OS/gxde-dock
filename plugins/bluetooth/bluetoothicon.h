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

#ifndef BLUETOOTHICON_H
#define BLUETOOTHICON_H

#include <QPixmap>
#include <QColor>

class QString;

/*
 * 从插件资源加载一个 symbolic SVG（白色 mask），渲染到指定大小并重上色为
 * color（保留 alpha），与 V23 quick-settings 的图标处理一致，保证在深色
 * 任务栏/弹窗上可见。
 * \param name 资源基名（不含 .svg），如 "bluetooth-active-symbolic_16"
 */
QPixmap bluetoothIcon(const QString &name, int size, qreal devicePixelRatio,
                      const QColor &color = Qt::white);

#endif // BLUETOOTHICON_H
