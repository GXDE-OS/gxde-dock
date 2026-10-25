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

#ifndef BLUETOOTHSTATUSWIDGET_H
#define BLUETOOTHSTATUSWIDGET_H

#include "dbus/dbusbluetooth.h"

#include <QWidget>

class DBusBluetooth;

#define BLUETOOTH_KEY "bluetooth"

class BluetoothStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BluetoothStatusWidget(DBusBluetooth *inter, QWidget *parent = nullptr);

    void refreshIcon();

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent *e) override;

private:
    QPixmap dockIcon() const;

private:
    DBusBluetooth *m_bluetoothInter;
};

#endif // BLUETOOTHSTATUSWIDGET_H
