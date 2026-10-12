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

#ifndef OUTPUTDEVICEWIDGET_H
#define OUTPUTDEVICEWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include <QVBoxLayout>

class DBusAudio;

class OutputDeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OutputDeviceWidget(DBusAudio *audioInter, QWidget *parent = nullptr);

    // 重新枚举输出设备（首次、切换默认设备、弹出窗口打开时调用）
    void refreshSinks();

signals:
    // 设备列表内容变化时发出，供外层重新计算悬浮窗高度
    void contentChanged() const;

private:
    DBusAudio *m_audioInter;
    QVBoxLayout *m_listLayout;
    QButtonGroup *m_group;
};

#endif // OUTPUTDEVICEWIDGET_H
