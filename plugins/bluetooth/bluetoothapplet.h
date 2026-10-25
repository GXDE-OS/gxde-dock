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

#ifndef BLUETOOTHAPPLET_H
#define BLUETOOTHAPPLET_H

#include "dbus/dbusbluetooth.h"

#include <QWidget>

class QScrollArea;
class QVBoxLayout;

// 点击弹出的蓝牙面板：UI 参考 V23 quick-settings 的蓝牙页
// （节标题+开关、已配对/可用设备列表、连接/断开、蓝牙设置入口）
class BluetoothApplet : public QWidget
{
    Q_OBJECT

public:
    explicit BluetoothApplet(DBusBluetooth *inter, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void showEvent(QShowEvent *e) override;

private:
    void rebuild();
    void openSettings();

private:
    DBusBluetooth *m_bluetoothInter;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

#endif // BLUETOOTHAPPLET_H
