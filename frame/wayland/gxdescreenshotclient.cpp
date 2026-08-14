/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
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

#include "gxdescreenshotclient.h"

#include "protocols/gxde-screenshot-v1-client-protocol.h"
#include "protocols/kywc-toplevel-v1-client-protocol.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <utility>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <wayland-client.h>

// DRM 四字符码（避免引入 libdrm 依赖）
#define DRM_FORMAT_XRGB8888 0x34325258
#define DRM_FORMAT_ARGB8888 0x34325241
#define DRM_FORMAT_XBGR8888 0x34324258
#define DRM_FORMAT_ABGR8888 0x34324241

#define DRM_FORMAT_MOD_LINEAR 0

// DRM_FORMAT_MOD_INVALID = 0x00FFFFFFFFFFFFFFULL
static const quint64 ModifierInvalid = (1ULL << 56) - 1;

// ---------------------------------------------------------------------------
// 帧(buffer)回调上下文
// ---------------------------------------------------------------------------
struct CaptureContext
{
    bool done = false;
    bool ok = false;
    int fd = -1;
    quint32 format = 0;
    quint32 width = 0;
    quint32 height = 0;
    quint32 offset = 0;
    quint32 stride = 0;
    quint64 modifier = 0;
    QImage image;
};

static void handleFrameFailed(void *data, struct gxde_screenshot_frame_v1 *)
{
    CaptureContext *ctx = static_cast<CaptureContext *>(data);
    ctx->done = true;
    ctx->ok = false;
    qWarning() << "(GxdeScreenshotClient) frame failed";
}

static void handleFrameCancelled(void *data, struct gxde_screenshot_frame_v1 *)
{
    CaptureContext *ctx = static_cast<CaptureContext *>(data);
    ctx->done = true;
    ctx->ok = false;
    qWarning() << "(GxdeScreenshotClient) frame cancelled";
}

static void handleFrameBuffer(void *data, struct gxde_screenshot_frame_v1 *,
                              int32_t fd, uint32_t format, uint32_t width,
                              uint32_t height, uint32_t offset, uint32_t stride,
                              uint32_t modifier_hi, uint32_t modifier_lo, uint32_t flags)
{
    Q_UNUSED(flags);
    CaptureContext *ctx = static_cast<CaptureContext *>(data);
    ctx->fd = fd;
    ctx->format = format;
    ctx->width = width;
    ctx->height = height;
    ctx->offset = offset;
    ctx->stride = stride;
    ctx->modifier = (quint64(modifier_hi) << 32) | quint64(modifier_lo);
}

static void handleFrameBufferWithPlane(void *, struct gxde_screenshot_frame_v1 *,
                                       uint32_t, int32_t, uint32_t, uint32_t)
{
    // 缩略图只需要第一个平面，忽略
}

static void handleFrameBufferDone(void *data, struct gxde_screenshot_frame_v1 *)
{
    CaptureContext *ctx = static_cast<CaptureContext *>(data);
    ctx->done = true;

    if (ctx->fd < 0 || ctx->width == 0 || ctx->height == 0) {
        qWarning() << "(GxdeScreenshotClient) buffer done without valid buffer";
        return;
    }

    // 只支持线性 buffer (shm 或 linear dmabuf)。带 tiling 修饰符的
    // dmabuf 直接 mmap 会得到错误像素，放弃并让调用方回退旧路径。
    if (ctx->modifier != DRM_FORMAT_MOD_LINEAR && ctx->modifier != ModifierInvalid) {
        qWarning() << "(GxdeScreenshotClient) unsupported tiled modifier"
                   << Qt::hex << ctx->modifier;
        close(ctx->fd);
        ctx->fd = -1;
        return;
    }

    QImage::Format qformat;
    switch (ctx->format) {
    case DRM_FORMAT_ARGB8888:
        qformat = QImage::Format_ARGB32;
        break;
    case DRM_FORMAT_XRGB8888:
        qformat = QImage::Format_RGB32;
        break;
    case DRM_FORMAT_ABGR8888:
        qformat = QImage::Format_RGBA8888;
        break;
    case DRM_FORMAT_XBGR8888:
        qformat = QImage::Format_RGBX8888;
        break;
    default:
        qWarning() << "(GxdeScreenshotClient) unsupported buffer format"
                   << Qt::hex << ctx->format;
        close(ctx->fd);
        ctx->fd = -1;
        return;
    }

    const size_t mapSize = size_t(ctx->offset) + size_t(ctx->stride) * ctx->height;
    void *mapped = mmap(nullptr, mapSize, PROT_READ, MAP_SHARED, ctx->fd, 0);
    if (mapped == MAP_FAILED) {
        qWarning() << "(GxdeScreenshotClient) mmap buffer failed";
        close(ctx->fd);
        ctx->fd = -1;
        return;
    }

    const uchar *pixels = static_cast<const uchar *>(mapped) + ctx->offset;
    QImage img(pixels, int(ctx->width), int(ctx->height), int(ctx->stride), qformat);
    // 拷贝一份，脱离 mmap 内存
    ctx->image = img.copy();

    munmap(mapped, mapSize);
    close(ctx->fd);
    ctx->fd = -1;

    ctx->ok = true;
}

static const struct gxde_screenshot_frame_v1_listener frameListener = {
    handleFrameFailed,
    handleFrameCancelled,
    handleFrameBuffer,
    handleFrameBufferWithPlane,
    handleFrameBufferDone,
};

// ---------------------------------------------------------------------------
// kywc toplevel 跟踪
// ---------------------------------------------------------------------------
struct ToplevelUserData
{
    GxdeScreenshotClient *client = nullptr;
    QString uuid;
};

static void toplevelHandleClosed(void *data, struct kywc_toplevel_v1 *toplevel)
{
    ToplevelUserData *ud = static_cast<ToplevelUserData *>(data);
    if (ud->client) {
        ud->client->removeToplevel(ud->uuid);
    }
    kywc_toplevel_v1_destroy(toplevel);
    delete ud;
}

static void toplevelHandleDone(void *, struct kywc_toplevel_v1 *) {}

static void toplevelHandleTitle(void *data, struct kywc_toplevel_v1 *, const char *title)
{
    ToplevelUserData *ud = static_cast<ToplevelUserData *>(data);
    if (ud->client) {
        ud->client->setToplevelTitle(ud->uuid, QString::fromUtf8(title));
    }
}

static void toplevelHandleAppId(void *data, struct kywc_toplevel_v1 *, const char *appId)
{
    ToplevelUserData *ud = static_cast<ToplevelUserData *>(data);
    if (ud->client) {
        ud->client->setToplevelAppId(ud->uuid, QString::fromUtf8(appId));
    }
}

static void toplevelHandlePrimaryOutput(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleWorkspaceEnter(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleWorkspaceLeave(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleCapabilities(void *, struct kywc_toplevel_v1 *, uint32_t) {}
static void toplevelHandleState(void *, struct kywc_toplevel_v1 *, uint32_t) {}
static void toplevelHandleParent(void *, struct kywc_toplevel_v1 *, struct kywc_toplevel_v1 *) {}
static void toplevelHandleIcon(void *, struct kywc_toplevel_v1 *, const char *) {}
static void toplevelHandleGeometry(void *, struct kywc_toplevel_v1 *, int32_t, int32_t, uint32_t, uint32_t) {}
static void toplevelHandlePid(void *, struct kywc_toplevel_v1 *, uint32_t) {}

static const struct kywc_toplevel_v1_listener toplevelListener = {
    toplevelHandleClosed,
    toplevelHandleDone,
    toplevelHandleTitle,
    toplevelHandleAppId,
    toplevelHandlePrimaryOutput,
    toplevelHandleWorkspaceEnter,
    toplevelHandleWorkspaceLeave,
    toplevelHandleCapabilities,
    toplevelHandleState,
    toplevelHandleParent,
    toplevelHandleIcon,
    toplevelHandleGeometry,
    toplevelHandlePid,
};

static void toplevelManagerHandleToplevel(void *data, struct kywc_toplevel_manager_v1 *,
                                          struct kywc_toplevel_v1 *toplevel, const char *uuid)
{
    GxdeScreenshotClient *client = static_cast<GxdeScreenshotClient *>(data);
    if (!client) {
        kywc_toplevel_v1_destroy(toplevel);
        return;
    }

    ToplevelUserData *ud = new ToplevelUserData;
    ud->client = client;
    ud->uuid = QString::fromUtf8(uuid);
    kywc_toplevel_v1_add_listener(toplevel, &toplevelListener, ud);
    client->addToplevel(ud->uuid);
}

static void toplevelManagerHandleFinished(void *data, struct kywc_toplevel_manager_v1 *manager)
{
    // stop 请求的应答：compositor 不再发送 toplevel 事件
    GxdeScreenshotClient *client = static_cast<GxdeScreenshotClient *>(data);
    if (client) {
        client->onToplevelManagerFinished();
    }
    kywc_toplevel_manager_v1_destroy(manager);
}

static const struct kywc_toplevel_manager_v1_listener toplevelManagerListener = {
    toplevelManagerHandleToplevel,
    toplevelManagerHandleFinished,
};

// ---------------------------------------------------------------------------
// registry 回调
// ---------------------------------------------------------------------------
static void registryHandleGlobal(void *data, wl_registry *registry, uint32_t name,
                                 const char *interface, uint32_t version)
{
    Q_UNUSED(version);
    GxdeScreenshotClient *client = static_cast<GxdeScreenshotClient *>(data);

    if (strcmp(interface, gxde_screenshot_manager_v1_interface.name) == 0) {
        client->bindScreenshotManager(registry, name);
    } else if (strcmp(interface, kywc_toplevel_manager_v1_interface.name) == 0) {
        client->bindToplevelManager(registry, name);
    }
}

static void registryHandleGlobalRemove(void *data, wl_registry *, uint32_t name)
{
    GxdeScreenshotClient *client = static_cast<GxdeScreenshotClient *>(data);
    client->handleGlobalRemove(name);
}

static const wl_registry_listener registryListener = {
    registryHandleGlobal,
    registryHandleGlobalRemove,
};

// ---------------------------------------------------------------------------
// GxdeScreenshotClient
// ---------------------------------------------------------------------------
GxdeScreenshotClient::GxdeScreenshotClient(QObject *parent)
    : QObject(parent)
{
}

GxdeScreenshotClient::~GxdeScreenshotClient()
{
    cleanup();
}

GxdeScreenshotClient *GxdeScreenshotClient::instance()
{
    static GxdeScreenshotClient *client = new GxdeScreenshotClient;
    return client;
}

void GxdeScreenshotClient::ensureInit()
{
    if (m_inited) {
        return;
    }
    m_inited = true;

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    m_display = static_cast<wl_display *>(native->nativeResourceForWindow("display", nullptr));
    if (!m_display) {
        qWarning() << "(GxdeScreenshotClient) no wayland display";
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    if (m_manager) {
        qInfo() << "(GxdeScreenshotClient) gxde_screenshot_manager_v1 available,"
                << "dock previews will use the Wayland protocol";
    }
}

void GxdeScreenshotClient::cleanup()
{
    // toplevel proxy 们在 closed 回调中自毁，这里只销毁 manager 与 registry
    if (m_toplevelManager) {
        kywc_toplevel_manager_v1_destroy(m_toplevelManager);
        m_toplevelManager = nullptr;
    }
    if (m_manager) {
        gxde_screenshot_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
}

bool GxdeScreenshotClient::available() const
{
    const_cast<GxdeScreenshotClient *>(this)->ensureInit();
    return m_manager != nullptr;
}

void GxdeScreenshotClient::bindScreenshotManager(wl_registry *registry, uint32_t name)
{
    m_manager = static_cast<gxde_screenshot_manager_v1 *>(
        wl_registry_bind(registry, name, &gxde_screenshot_manager_v1_interface, 1));
}

void GxdeScreenshotClient::bindToplevelManager(wl_registry *registry, uint32_t name)
{
    m_toplevelManager = static_cast<kywc_toplevel_manager_v1 *>(
        wl_registry_bind(registry, name, &kywc_toplevel_manager_v1_interface, 1));
    kywc_toplevel_manager_v1_add_listener(m_toplevelManager, &toplevelManagerListener, this);
}

void GxdeScreenshotClient::handleGlobalRemove(uint32_t name)
{
    Q_UNUSED(name);
    // 保守处理：global 被移除后清空 manager，让调用方回退旧路径
    if (m_manager) {
        gxde_screenshot_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_toplevelManager) {
        kywc_toplevel_manager_v1_destroy(m_toplevelManager);
        m_toplevelManager = nullptr;
    }
}

void GxdeScreenshotClient::addToplevel(const QString &uuid)
{
    m_toplevels.insert(uuid, ToplevelInfo());
}

void GxdeScreenshotClient::removeToplevel(const QString &uuid)
{
    m_toplevels.remove(uuid);
}

void GxdeScreenshotClient::setToplevelTitle(const QString &uuid, const QString &title)
{
    auto it = m_toplevels.find(uuid);
    if (it != m_toplevels.end()) {
        it->title = title;
    }
}

void GxdeScreenshotClient::setToplevelAppId(const QString &uuid, const QString &appId)
{
    auto it = m_toplevels.find(uuid);
    if (it != m_toplevels.end()) {
        it->appId = appId;
    }
}

void GxdeScreenshotClient::onToplevelManagerFinished()
{
    m_toplevelManager = nullptr;
}

void GxdeScreenshotClient::refreshToplevels()
{
    ensureInit();
    if (!m_display) {
        return;
    }
    // 处理积压的 toplevel 事件（新建窗口、标题/app_id 变更、关闭）
    wl_display_flush(m_display);
    wl_display_roundtrip(m_display);
}

QString GxdeScreenshotClient::findWindowUuid(const QString &appId, const QString &title) const
{
    const QString normalizedApp = appId.toLower();

    // 1. app_id(忽略大小写) + 标题精确匹配
    QString titleOnlyUuid;
    int titleOnlyCount = 0;
    for (auto it = m_toplevels.constBegin(); it != m_toplevels.constEnd(); ++it) {
        const ToplevelInfo &info = it.value();
        if (info.title.isEmpty()) {
            continue;
        }
        if (info.title == title) {
            ++titleOnlyCount;
            titleOnlyUuid = it.key();
            if (!appId.isEmpty() && info.appId.toLower() == normalizedApp) {
                return it.key();
            }
        }
    }

    // 2. 退化为标题唯一匹配
    if (titleOnlyCount == 1) {
        return titleOnlyUuid;
    }

    return QString();
}

bool GxdeScreenshotClient::captureWindowThumbnail(const QString &appId, const QString &title,
                                                  quint32 maxWidth, quint32 maxHeight, QImage *out)
{
    if (!out) {
        return false;
    }

    ensureInit();
    if (!m_manager || !m_display) {
        return false;
    }

    refreshToplevels();
    const QString uuid = findWindowUuid(appId, title);
    if (uuid.isEmpty()) {
        qDebug() << "(GxdeScreenshotClient) no uuid for window"
                 << appId << title;
        return false;
    }

    const QByteArray uuidUtf8 = uuid.toUtf8();
    qDebug() << "(GxdeScreenshotClient) capture thumbnail, uuid=" << uuid
             << "appId=" << appId << "title=" << title;

    // 注意: app_id 参数传空。合成器端用 strcmp 精确匹配 app_id，
    // dock 侧解析出的 app_id 与 WM_CLASS 大小写可能不一致，会导致匹配失败。
    // 协议说明: "It is FINE if app_id is empty, then we will only grab the
    // thumbnail for single window."
    struct gxde_screenshot_frame_v1 *frame = gxde_screenshot_manager_v1_capture_window_thumbnail(
        m_manager, "", uuidUtf8.constData(),
        maxWidth, maxHeight, 0);

    CaptureContext ctx;
    gxde_screenshot_frame_v1_add_listener(frame, &frameListener, &ctx);
    wl_display_flush(m_display);

    // 合成器一般在下一帧渲染后回传 buffer，这里阻塞等待最多约 1 秒。
    // 用 prepare_read + poll 实现带超时的等待（不能用 wl_display_poll，那是内部 API）。
    QElapsedTimer timer;
    timer.start();
    while (!ctx.done && timer.elapsed() < 1000) {
        wl_display_flush(m_display);
        if (wl_display_prepare_read(m_display) != 0) {
            // 已有事件排队，直接分发
            wl_display_dispatch_pending(m_display);
            continue;
        }

        struct pollfd pfd;
        pfd.fd = wl_display_get_fd(m_display);
        pfd.events = POLLIN;
        pfd.revents = 0;
        const int ret = poll(&pfd, 1, 50);
        if (ret <= 0) {
            wl_display_cancel_read(m_display);
            continue;
        }

        wl_display_read_events(m_display);
        wl_display_dispatch_pending(m_display);
    }

    gxde_screenshot_frame_v1_release_buffer(frame, 0);
    gxde_screenshot_frame_v1_destroy(frame);
    wl_display_flush(m_display);

    if (!ctx.ok) {
        qWarning() << "(GxdeScreenshotClient) capture timed out or failed,"
                   << "done=" << ctx.done << "ok=" << ctx.ok;
        return false;
    }

    *out = std::move(ctx.image);
    return !out->isNull();
}
