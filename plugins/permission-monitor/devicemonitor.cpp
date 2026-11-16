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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

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

} // namespace

DeviceMonitor::DeviceMonitor(QObject *parent)
    : QObject(parent)
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &DeviceMonitor::refreshUsage);
}

void DeviceMonitor::start(int intervalMs)
{
    m_refreshTimer->setInterval(intervalMs);
    m_refreshTimer->start();
    refreshUsage();
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

    const bool changed = usages[Camera] != m_usages[Camera]
            || usages[Microphone] != m_usages[Microphone];
    m_usages[Camera] = usages[Camera];
    m_usages[Microphone] = usages[Microphone];

    if (changed) {
        emit usageChanged();
    }
}
