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

#include "micstreammonitor.h"

#include <QMetaObject>

#include <pulse/subscribe.h>

MicStreamMonitor::MicStreamMonitor(QObject *parent)
    : QObject(parent)
{
}

void MicStreamMonitor::start()
{
    // 防止重复启动
    if (m_mainloop) {
        return;
    }

    m_mainloop = pa_threaded_mainloop_new();
    if (!m_mainloop) {
        return;
    }

    m_mainloopApi = pa_threaded_mainloop_get_api(m_mainloop);

    pa_proplist *proplist = pa_proplist_new();
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, "gxde-dock permission-monitor");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, "com.gxde.dock.permission-monitor");

    m_context = pa_context_new_with_proplist(m_mainloopApi, nullptr, proplist);
    pa_proplist_free(proplist);

    if (!m_context) {
        return;
    }

    pa_context_set_state_callback(m_context, onContextState, this);

    pa_threaded_mainloop_start(m_mainloop);

    pa_threaded_mainloop_lock(m_mainloop);
    if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
        // 连接失败不致命：PA_CONTEXT_NOFAIL 下上下文会保持 connecting，
        // 状态回调在恢复后仍会触发
    }
    pa_threaded_mainloop_unlock(m_mainloop);
}

MicStreamMonitor::~MicStreamMonitor()
{
    if (!m_mainloop) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainloop);
    if (m_context) {
        pa_context_set_state_callback(m_context, nullptr, nullptr);
        pa_context_set_subscribe_callback(m_context, nullptr, nullptr);
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    pa_threaded_mainloop_unlock(m_mainloop);

    pa_threaded_mainloop_stop(m_mainloop);
    pa_threaded_mainloop_free(m_mainloop);
    m_mainloop = nullptr;
}

QStringList MicStreamMonitor::recordingApps() const
{
    return m_apps;
}

void MicStreamMonitor::updateApps()
{
    QStringList apps;
    for (const StreamInfo &stream : m_streams) {
        // 排除 monitor 虚拟源（录制桌面输出的情况），只统计真实麦克风录音
        if (stream.fromMonitor) {
            continue;
        }

        const QString app = stream.appName.isEmpty() ? stream.binary : stream.appName;
        if (!app.isEmpty() && !apps.contains(app)) {
            apps.append(app);
        }
    }

    if (apps != m_apps) {
        m_apps = apps;
        emit recordingAppsChanged();
    }
}

void MicStreamMonitor::onContextState(pa_context *c, void *userdata)
{
    auto *self = static_cast<MicStreamMonitor *>(userdata);

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY: {
        QMetaObject::invokeMethod(self, [self]() {
            self->m_streams.clear();
            self->updateApps();
        }, Qt::QueuedConnection);

        pa_context_set_subscribe_callback(c, onSubscribe, self);
        pa_operation *o = pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
                                               nullptr, nullptr);
        if (o) pa_operation_unref(o);

        o = pa_context_get_source_output_info_list(c, onSourceOutputInfo, self);
        if (o) pa_operation_unref(o);
        break;
    }

    case PA_CONTEXT_FAILED:
        // 连接失败（服务器不可用）：清空应用列表，交由设备级检测兜底
        QMetaObject::invokeMethod(self, [self]() {
            self->m_streams.clear();
            self->updateApps();
        }, Qt::QueuedConnection);
        break;

    default:
        break;
    }
}

void MicStreamMonitor::onSubscribe(pa_context *c, pa_subscription_event_type_t t,
                                   uint32_t index, void *userdata)
{
    auto *self = static_cast<MicStreamMonitor *>(userdata);

    const uint32_t facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    const uint32_t type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    if (facility != PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
        return;
    }

    if (type == PA_SUBSCRIPTION_EVENT_REMOVE) {
        QMetaObject::invokeMethod(self, [self, index]() {
            for (int i = 0; i < self->m_streams.size(); ++i) {
                if (self->m_streams[i].index == index) {
                    self->m_streams.removeAt(i);
                    break;
                }
            }
            self->updateApps();
        }, Qt::QueuedConnection);
        return;
    }

    pa_operation *o = pa_context_get_source_output_info(c, index, onSourceOutputInfo, self);
    if (o) pa_operation_unref(o);
}

void MicStreamMonitor::onSourceOutputInfo(pa_context *,
                                          const pa_source_output_info *i,
                                          int eol, void *userdata)
{
    auto *self = static_cast<MicStreamMonitor *>(userdata);
    if (eol < 0) {
        return;
    }
    if (eol > 0) {
        return;
    }

    const uint32_t index = i->index;
    const char *appRaw = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME);
    const char *binRaw = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);

    StreamInfo info;
    info.index = index;
    info.appName = appRaw ? QString::fromUtf8(appRaw) : QString();
    info.binary = binRaw ? QString::fromUtf8(binRaw) : QString();

    // source_output_info 的 source 字段是源索引而非源名，monitor 源通过
    // proplist 属性判定（这两个属性由服务器写入，客户端头文件未提供宏）：
    // "stream.capture.sink" 为 "true" 表示录制 sink 的 monitor 源；
    // "stream.device" 为录制目标源名，monitor 源名以 .monitor 结尾
    const char *capSink = pa_proplist_gets(i->proplist, "stream.capture.sink");
    const char *streamDevice = pa_proplist_gets(i->proplist, "stream.device");
    info.fromMonitor = (capSink && QByteArray(capSink) == QByteArrayLiteral("true"))
            || (streamDevice && QString::fromUtf8(streamDevice).endsWith(QStringLiteral(".monitor")));

    QMetaObject::invokeMethod(self, [self, info]() {
        for (int j = 0; j < self->m_streams.size(); ++j) {
            if (self->m_streams[j].index == info.index) {
                self->m_streams[j] = info;
                self->updateApps();
                return;
            }
        }
        self->m_streams.append(info);
        self->updateApps();
    }, Qt::QueuedConnection);
}
