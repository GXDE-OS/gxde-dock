/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
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

#ifndef MICSTREAMMONITOR_H
#define MICSTREAMMONITOR_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/thread-mainloop.h>

// 监听本会话 PulseAudio/PipeWire 服务器中的录音流（source outputs），
// 识别正在录制麦克风的应用（PipeWire 完全兼容 PulseAudio 协议）。
//
// 仅统计录制真实麦克风（非 monitor 虚拟源）的流——录制 monitor 源属于
// 录制桌面输出，不涉及麦克风。
//
// PA 回调运行在 PA 线程上，通过 QMetaObject::invokeMethod(QueuedConnection)
// 切回 Qt 主线程后再更新缓存。
class MicStreamMonitor : public QObject
{
    Q_OBJECT

public:
    explicit MicStreamMonitor(QObject *parent = nullptr);
    ~MicStreamMonitor() override;

    // 启动 PulseAudio 连接。必须在 init 之后调用：dock 加载 .so 时会创建
    // 一个从不 init 的根实例（仅作为工厂接口使用），构造即连接会让根实例
    // 也产生状态事件并访问未初始化的 m_proxyInter 导致崩溃
    void start();

    // 正在录制麦克风的应用名列表（优先 application.name，其次
    // application.process.binary），仅在 Qt 主线程调用
    QStringList recordingApps() const;

signals:
    void recordingAppsChanged();

private:
    struct StreamInfo {
        uint32_t index;
        QString appName;
        QString binary;
        bool fromMonitor = false;
    };

    void updateApps();

    // ---- PA 回调（PA 线程） ----
    static void onContextState(pa_context *c, void *userdata);
    static void onSubscribe(pa_context *c, pa_subscription_event_type_t t,
                            uint32_t index, void *userdata);
    static void onSourceOutputInfo(pa_context *c, const pa_source_output_info *i,
                                   int eol, void *userdata);

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_mainloop_api *m_mainloopApi = nullptr;
    pa_context *m_context = nullptr;

    // 以下成员仅在 Qt 主线程访问（由 QueuedConnection 回调更新）
    QList<StreamInfo> m_streams;
    QStringList m_apps;
};

#endif // MICSTREAMMONITOR_H
