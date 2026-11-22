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

#include "devicemonitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#define DEVICE_MONITOR_SERVICE      "com.gxde.daemon.devicemonitor"
#define DEVICE_MONITOR_PATH         "/com/gxde/daemon/devicemonitor"
#define DEVICE_MONITOR_INTERFACE    "com.gxde.daemon.devicemonitor"

namespace {

// /dev/video* 为摄像头设备，/dev/snd/pcmC*D*c 为 ALSA 采集（录音）设备，
// c/p 后缀分别表示 capture/playback，其余（如 controlC*、hwC*D*、seq、timer）
// 会被混音器等长期持有，不能作为占用依据。
const QRegularExpression &cameraPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^/dev/video\\d+$"));
    return pattern;
}

const QRegularExpression &microphonePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^/dev/snd/pcmC\\d+D\\d+c$"));
    return pattern;
}

// 音频服务进程：它们持有设备往往只是因为其它应用的录音流经过，
// 已识别出具体录音应用时不应展示为占用者
const QStringList &audioServerProcessNames()
{
    static const QStringList names = {
        QStringLiteral("pipewire"),
        QStringLiteral("pipewire-pulse"),
        QStringLiteral("pulseaudio"),
    };
    return names;
}

} // namespace

DeviceMonitor::DeviceMonitor(QObject *parent)
    : QObject(parent)
    , m_refreshTimer(new QTimer(this))
    , m_micStreamMonitor(new MicStreamMonitor(this))
{
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &DeviceMonitor::refreshUsage);
    connect(m_micStreamMonitor, &MicStreamMonitor::recordingAppsChanged,
            this, &DeviceMonitor::onMicAppsChanged);
}

void DeviceMonitor::start(int intervalMs)
{
    m_refreshTimer->setInterval(intervalMs);

    // 录音流检测同样在此启动（构造时不启动：dock 加载时的根实例不会
    // 被调用 init，若构造即产生状态事件将访问未初始化的 m_proxyInter）
    m_micStreamMonitor->start();

    // 先做一次本地扫描提供初始状态（无需等待 D-Bus 服务），随后尝试切换到
    // D-Bus 数据源（可覆盖所有用户的进程占用）
    refreshUsage();
    requestDBusUsage();
}

bool DeviceMonitor::deviceInUse(DeviceType type) const
{
    return m_usages[type].inUse;
}

QStringList DeviceMonitor::usingProcesses(DeviceType type) const
{
    return m_usages[type].processes;
}

QString DeviceMonitor::processName(int pid)
{
    QFile commFile(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!commFile.open(QFile::ReadOnly))
        return QString();

    QString name = QString::fromUtf8(commFile.readAll()).trimmed();
    commFile.close();
    return name;
}

void DeviceMonitor::refreshUsage()
{
    DeviceUsage usages[2];
    const QRegularExpression patterns[2] = { cameraPattern(), microphonePattern() };

    const QFileInfoList procEntries = QDir(QStringLiteral("/proc"))
            .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : procEntries) {
        bool pidValid = false;
        const int pid = entry.fileName().toInt(&pidValid);
        if (!pidValid) {
            continue;
        }

        // 其它用户（含 root）进程的 fd 目录不可读，entryList 返回空列表，静默跳过
        // fd 条目是指向设备/文件/管道等的符号链接，需包含 System 过滤器以覆盖
        // 链接目标为字符设备（/dev/video*、/dev/snd/*）的情况
        const QDir fdDir(entry.filePath() + QStringLiteral("/fd"));
        const QStringList fds = fdDir.entryList(QDir::Dirs | QDir::Files | QDir::System
                                                | QDir::Hidden | QDir::NoDotAndDotDot);
        for (const QString &fd : fds) {
            const QString target = QFile::symLinkTarget(fdDir.filePath(fd));
            if (target.isEmpty()) {
                continue;
            }

            for (int type = Camera; type <= Microphone; ++type) {
                if (target.contains(patterns[type])) {
                    usages[type].inUse = true;
                    const QString name = processName(pid);
                    if (!name.isEmpty()) {
                        usages[type].processes.append(name);
                    }
                }
            }
        }
    }

    usages[Camera].processes.removeDuplicates();
    usages[Microphone].processes.removeDuplicates();

    m_deviceUsages[Camera] = usages[Camera];
    m_deviceUsages[Microphone] = usages[Microphone];
    combineUsage();
}

void DeviceMonitor::combineUsage()
{
    DeviceUsage usages[2] = { m_deviceUsages[Camera], m_deviceUsages[Microphone] };

    // PulseAudio/PipeWire 录音流检测：识别真正使用麦克风的应用。
    // 设备级检测在音频服务架构下只能看到持有设备的音频服务进程，
    // 已识别出具体录音应用时，应用名合并展示并过滤掉音频服务进程名
    const QStringList micApps = m_micStreamMonitor->recordingApps();
    if (!micApps.isEmpty()) {
        usages[Microphone].inUse = true;
        for (const QString &server : audioServerProcessNames()) {
            usages[Microphone].processes.removeAll(server);
        }
        for (const QString &app : micApps) {
            if (!usages[Microphone].processes.contains(app)) {
                usages[Microphone].processes.prepend(app);
            }
        }
    }

    applyUsage(usages[Camera], usages[Microphone]);
}

void DeviceMonitor::applyUsage(const DeviceUsage &camera, const DeviceUsage &microphone)
{
    const bool changed = camera != m_usages[Camera]
            || microphone != m_usages[Microphone];
    m_usages[Camera] = camera;
    m_usages[Microphone] = microphone;

    if (changed) {
        emit usageChanged();
    }
}

void DeviceMonitor::requestDBusUsage()
{
    // 信号 match 只注册一次，避免服务重启时叠加重复连接
    if (!m_signalsConnected) {
        m_signalsConnected = true;

        // 订阅系统服务的状态变化信号（服务尚未启动时 match 也会注册成功，
        // 服务启动后信号自动到达）
        QDBusConnection bus = QDBusConnection::systemBus();
        bus.connect(QStringLiteral(DEVICE_MONITOR_SERVICE), QStringLiteral(DEVICE_MONITOR_PATH),
                    QStringLiteral(DEVICE_MONITOR_INTERFACE), QStringLiteral("UsageChanged"),
                    this, SLOT(onDBusUsageChanged(bool,bool,QStringList,QStringList)));
        // 服务启动/重启后重新获取一次完整状态（含切换数据源）
        bus.connect(QString(), QString(), QStringLiteral("org.freedesktop.DBus"),
                    QStringLiteral("NameOwnerChanged"),
                    this, SLOT(onDBusServiceOwnerChanged(QString,QString,QString)));
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
                QStringLiteral(DEVICE_MONITOR_SERVICE), QStringLiteral(DEVICE_MONITOR_PATH),
                QStringLiteral(DEVICE_MONITOR_INTERFACE), QStringLiteral("GetUsage"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
                QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &DeviceMonitor::onDBusCallFinished);
}

void DeviceMonitor::onDBusCallFinished()
{
    QDBusPendingCallWatcher *watcher = static_cast<QDBusPendingCallWatcher *>(sender());
    watcher->deleteLater();

    QDBusPendingReply<bool, bool, QStringList, QStringList> reply = *watcher;
    if (!reply.isValid()) {
        // 系统服务不可用（未安装或启动失败）：保持/恢复本地轮询，
        // 仅能检测当前用户的设备占用
        if (!m_refreshTimer->isActive()) {
            m_refreshTimer->start();
        }
        return;
    }

    // D-Bus 服务可用：停止本地轮询，状态由信号驱动
    m_refreshTimer->stop();
    onDBusUsageChanged(reply.argumentAt(0).toBool(),
                       reply.argumentAt(1).toBool(),
                       reply.argumentAt(2).toStringList(),
                       reply.argumentAt(3).toStringList());
}

void DeviceMonitor::onDBusUsageChanged(bool cameraInUse, bool microphoneInUse,
                                       const QStringList &cameraProcs, const QStringList &microphoneProcs)
{
    DeviceUsage usages[2];
    usages[Camera].inUse = cameraInUse;
    usages[Camera].processes = cameraProcs;
    usages[Microphone].inUse = microphoneInUse;
    usages[Microphone].processes = microphoneProcs;

    m_deviceUsages[Camera] = usages[Camera];
    m_deviceUsages[Microphone] = usages[Microphone];
    combineUsage();
}

void DeviceMonitor::onMicAppsChanged()
{
    combineUsage();
}

void DeviceMonitor::onDBusServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)

    // 服务启动/重启时重新获取状态（GetUsage 成功会停止本地轮询，
    // 失败则维持当前模式不受影响）
    if (name == QLatin1String(DEVICE_MONITOR_SERVICE) && !newOwner.isEmpty()) {
        requestDBusUsage();
    }
}
