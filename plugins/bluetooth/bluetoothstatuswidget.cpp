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

#include "bluetoothstatuswidget.h"
#include "dbus/dbusbluetooth.h"
#include "bluetoothicon.h"

#include <QPainter>
#include <QApplication>

BluetoothStatusWidget::BluetoothStatusWidget(DBusBluetooth *inter, QWidget *parent)
    : QWidget(parent)
    , m_bluetoothInter(inter)
{
    connect(m_bluetoothInter, &DBusBluetooth::adaptersChanged, this, static_cast<void (QWidget::*)()>(&QWidget::update));
    connect(m_bluetoothInter, &DBusBluetooth::devicesChanged, this, static_cast<void (QWidget::*)()>(&QWidget::update));
}

void BluetoothStatusWidget::refreshIcon()
{
    update();
}

QSize BluetoothStatusWidget::sizeHint() const
{
    // 与其它插件保持一致的方形图标尺寸
    return QSize(26, 26);
}

void BluetoothStatusWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const QPixmap icon = dockIcon();

    QPainter painter(this);
    const QRectF rf = QRectF(rect());
    const QRectF rfp = QRectF(icon.rect());
    const QPointF iconPos = rf.center() - rfp.center() / icon.devicePixelRatioF();
    painter.drawPixmap(iconPos, icon);
}

QPixmap BluetoothStatusWidget::dockIcon() const
{
    const QString iconName = m_bluetoothInter->isPowered()
            ? QStringLiteral("bluetooth-active-symbolic_16")
            : QStringLiteral("bluetooth-disable-symbolic_16");
    return bluetoothIcon(iconName, 16, devicePixelRatioF());
}
