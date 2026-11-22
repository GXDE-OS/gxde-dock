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

#include "micstreammonitor.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

class QDBusPendingCallWatcher;

// 监视摄像头/麦克风等设备的占用状态。
//
// 数据源组合：
// 1. 系统 D-Bus 服务 com.gxde.daemon.devicemonitor（root 权限，可检测所有
//    用户与系统服务进程对设备的占用），不可用时降级为本地扫描 /proc；
// 2. 本会话 PulseAudio/PipeWire 录音流检测（MicStreamMonitor），识别真正
//    使用麦克风的应用（设备级检测只能看到持有设备的音频服务进程）。
class DeviceMonitor : public QObject
{
    Q_OBJECT

public:
    enum DeviceType {
        Camera = 0,
        Microphone = 1,
    };

    explicit DeviceMonitor(QObject *parent = nullptr);

    // 启动检测：先做一次本地扫描提供初始状态，随后尝试切换到 D-Bus 数据源
    void start(int intervalMs = 1000);

    bool deviceInUse(DeviceType type) const;
    // 正在占用设备的进程名列表（可能为空：进程已退出或无法读取）
    QStringList usingProcesses(DeviceType type) const;

signals:
    // 占用状态或占用进程发生变化时发出
    void usageChanged();

private slots:
    // 降级模式的本地扫描（定时器驱动）
    void refreshUsage();
    void onDBusUsageChanged(bool cameraInUse, bool microphoneInUse,
                            const QStringList &cameraProcs, const QStringList &microphoneProcs);
    void onDBusCallFinished();
    void onDBusServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onMicAppsChanged();

private:
    struct DeviceUsage {
        bool inUse = false;
        QStringList processes;

        bool operator==(const DeviceUsage &other) const
        { return inUse == other.inUse && processes == other.processes; }
        bool operator!=(const DeviceUsage &other) const { return !(*this == other); }
    };

    // 结合设备级占用（m_deviceUsages）与录音流应用列表计算最终状态
    void combineUsage();
    void applyUsage(const DeviceUsage &camera, const DeviceUsage &microphone);
    void requestDBusUsage();
    static QString processName(int pid);

    QTimer *m_refreshTimer;
    // 设备级占用状态（来自 D-Bus 服务或本地扫描），最终状态经 combineUsage 计算
    DeviceUsage m_deviceUsages[2];
    DeviceUsage m_usages[2];
    MicStreamMonitor *m_micStreamMonitor;
    bool m_signalsConnected = false;
};

#endif // DEVICEMONITOR_H
