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

class QDBusPendingCallWatcher;

// 监视摄像头/麦克风等设备的占用状态。
//
// 优先通过系统 D-Bus 服务 com.gxde.daemon.devicemonitor（root 权限，可检测
// 所有用户与系统服务进程的占用）获取状态并订阅变化信号；该服务不可用（未
// 安装或启动失败）时自动降级为本地扫描 /proc/<pid>/fd，仅能覆盖当前用户
// 可见进程（无需 root）。
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

private:
    struct DeviceUsage {
        bool inUse = false;
        QStringList processes;

        bool operator==(const DeviceUsage &other) const
        { return inUse == other.inUse && processes == other.processes; }
        bool operator!=(const DeviceUsage &other) const { return !(*this == other); }
    };

    void applyUsage(const DeviceUsage &camera, const DeviceUsage &microphone);
    void requestDBusUsage();
    static QString processName(int pid);

    QTimer *m_refreshTimer;
    DeviceUsage m_usages[2];
    bool m_signalsConnected = false;
};

#endif // DEVICEMONITOR_H
