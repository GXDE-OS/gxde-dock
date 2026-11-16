/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#ifndef DEVICEMONITOR_H
#define DEVICEMONITOR_H

#include <QObject>
#include <QStringList>
#include <QTimer>

// 监视摄像头/麦克风等设备的占用状态。
//
// 检测原理：扫描 /proc/<pid>/fd 下各符号链接的指向（仅需当前用户权限，无需 root），
// 链接目标指向 /dev/video* 视为摄像头被占用，指向 /dev/snd/pcmC*D*c
// （c 结尾的 ALSA 采集设备）视为麦克风被占用。
// 用户态程序与 PulseAudio/PipeWire 服务均在当前用户下运行，因此可以探测到。
class DeviceMonitor : public QObject
{
    Q_OBJECT

public:
    enum DeviceType {
        Camera = 0,
        Microphone = 1,
    };

    explicit DeviceMonitor(QObject *parent = nullptr);

    // 启动定时检测，并立即检测一次
    void start(int intervalMs = 1000);

    bool deviceInUse(DeviceType type) const;
    // 正在占用设备的进程名列表（可能为空：进程已退出或无法读取）
    QStringList usingProcesses(DeviceType type) const;

signals:
    // 占用状态或占用进程发生变化时发出
    void usageChanged();

private slots:
    void refreshUsage();

private:
    struct DeviceUsage {
        bool inUse = false;
        QStringList processes;

        bool operator==(const DeviceUsage &other) const
        { return inUse == other.inUse && processes == other.processes; }
        bool operator!=(const DeviceUsage &other) const { return !(*this == other); }
    };

    static QString processName(int pid);

    QTimer *m_refreshTimer;
    DeviceUsage m_usages[2];
};

#endif // DEVICEMONITOR_H
