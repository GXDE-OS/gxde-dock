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

#include "appsnapshot.h"
#include "../../util/waylandhelper.h"
#include "previewcontainer.h"
#include "../../dbus/dockdbusnames.h"

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/shm.h>

#include <QPainter>
#include <QVBoxLayout>
#include <QSizeF>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QGuiApplication>

// QX11Info is NOT avaliable in Qt6, using own helper...
#include "../../util/x11helper.h"

struct SHMInfo
{
    long shmid;
    long width;
    long height;
    long bytesPerLine;
    long format;

    struct Rect
    {
        long x;
        long y;
        long width;
        long height;
    } rect;
};

static bool windowPreviewsAvailable(DWindowManagerHelper *helper)
{
    return Wayland::isWaylandSession() || helper->hasComposite();
}

AppSnapshot::AppSnapshot(const WId wid, QWidget *parent)
    : QWidget(parent)
    , m_wid(wid)
    , m_waylandCapturePending(false)
    , m_title(new TipsWidget)
    , m_waitLeaveTimer(new QTimer(this))
    , m_closeBtn2D(new DImageButton)
    , m_wmHelper(DWindowManagerHelper::instance())
{
    m_closeBtn2D->setFixedSize(24, 24);
    m_closeBtn2D->setNormalPic(":/icons/resources/close_round_normal.svg");
    m_closeBtn2D->setHoverPic(":/icons/resources/close_round_hover.svg");
    m_closeBtn2D->setPressPic(":/icons/resources/close_round_press.svg");
    m_closeBtn2D->setVisible(false);
    m_title->setObjectName("AppSnapshotTitle");

    QHBoxLayout *centralLayout = new QHBoxLayout;
    centralLayout->addWidget(m_title);
    centralLayout->addWidget(m_closeBtn2D);
    centralLayout->setSpacing(5);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    centralLayout->setAlignment(m_closeBtn2D, Qt::AlignRight);

    setLayout(centralLayout);
    setAcceptDrops(true);
    resize(SNAP_WIDTH, SNAP_HEIGHT);

    connect(m_closeBtn2D, &DImageButton::clicked, this, &AppSnapshot::closeWindow, Qt::QueuedConnection);
    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &AppSnapshot::compositeChanged, Qt::QueuedConnection);
    QTimer::singleShot(1, this, &AppSnapshot::compositeChanged);
}

void AppSnapshot::closeWindow() const
{
    if (Wayland::isWaylandSession()) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            dockDBusService(),
            dockDBusManagerPath(),
            QString::fromLatin1(dockDBusManagerInterface()),
            QStringLiteral("CloseWindow"));
        message << quint32(m_wid);
        QDBusConnection::sessionBus().asyncCall(message);
        return;
    }

    const auto display = x11Display();

    XEvent e;

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.window = m_wid;
    e.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
    e.xclient.format = 32;
    e.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", false);
    e.xclient.data.l[1] = CurrentTime;

    XSendEvent(display, m_wid, false, NoEventMask, &e);
    XFlush(display);
}

void AppSnapshot::compositeChanged() const
{
    const bool composite = windowPreviewsAvailable(m_wmHelper);

    m_title->setVisible(!composite);

    QTimer::singleShot(1, this, &AppSnapshot::fetchSnapshot);
}

void AppSnapshot::setWindowInfo(const WindowInfo &info)
{
    m_windowInfo = info;

    m_title->setText(m_windowInfo.title);
}

void AppSnapshot::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);

    if (windowPreviewsAvailable(m_wmHelper))
        emit entered(m_wid);
}

// Getting screenshot from KYWC/gxde-wlcom
void AppSnapshot::fetchWaylandSnapshot() {
    if (m_waylandCapturePending) {
        return;
    }

    const QString dir = QStringLiteral("%1/gxde-dock")
        .arg(QStandardPaths::writableLocation(
            QStandardPaths::RuntimeLocation));
    if (!QDir().mkpath(dir)) {
        qWarning() << "(Dock) Snapshot: Failed to create preview dir" << dir;
        m_title->setVisible(true);
        return;
    }

    const QString path = QStringLiteral("%1/preview-%2.png").arg(dir)
        .arg(quint32(m_wid));

    QDBusMessage message = QDBusMessage::createMethodCall(
        dockDBusService(),
        dockDBusManagerPath(),
        QString::fromLatin1(dockDBusManagerInterface()),
        QStringLiteral("CaptureWindow"));
    message << quint32(m_wid) << path;

    m_waylandCapturePending = true;

    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, path](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_waylandCapturePending = false;

        const QDBusPendingReply<bool> reply = *w;
        if (reply.isError() || !reply.value()) {
            qWarning() << "(Dock) Snapshot: Failed to capture Wayland window"
                << m_wid << reply.error().message();
            m_title->setVisible(true);
            return;
        }

        QImage image;
        const bool loaded = image.load(path);
        QFile::remove(path);
        if (!loaded || image.isNull()) {
            qWarning() << "(Dock) Snapshot: Failed to load window capture"
                << path;
            m_title->setVisible(true);
            return;
        }

        applyWaylandSnapshot(image);
    });
}

void AppSnapshot::applyWaylandSnapshot(const QImage& image) {
    m_snapshot = image;
    m_snapshotSrcRect = m_snapshot.rect();
    m_title->setVisible(false);

    QSizeF size(rect().marginsRemoved(QMargins(8, 8, 8, 8)).size());
    const auto ratio = devicePixelRatioF();
    size = m_snapshotSrcRect.size().scaled(size * ratio, Qt::KeepAspectRatio);
    const qreal scale = qreal(size.width()) / m_snapshotSrcRect.width();
    m_snapshot = m_snapshot.scaled(
        qRound(m_snapshot.width() * scale),
        qRound(m_snapshot.height() * scale), Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);
    m_snapshotSrcRect = QRectF(QPointF(0, 0), size);
    m_snapshot.setDevicePixelRatio(ratio);
    update();
}

void AppSnapshot::fetchSnapshot()
{
    if (!windowPreviewsAvailable(m_wmHelper))
        return;

    if (Wayland::isWaylandSession()) {
        fetchWaylandSnapshot();
        return;
    }

    QImage qimage;
    SHMInfo *info = nullptr;
    uchar *image_data = nullptr;
    XImage *ximage = nullptr;
    unsigned char *prop_to_return_gtk = nullptr;

    do {
        // get window image from shm(only for deepin app)
        info = getImageDSHM();
        if (info) {
            qDebug() << "get Image from dxcbplugin SHM...";
            //qDebug() << info->shmid << info->width << info->height << info->bytesPerLine << info->format << info->rect.x << info->rect.y << info->rect.width << info->rect.height;
            image_data = (uchar*)shmat(info->shmid, 0, 0);
            if ((qint64)image_data != -1) {
                m_snapshot = QImage(image_data, info->width, info->height, info->bytesPerLine, (QImage::Format)info->format);
                m_snapshotSrcRect = QRect(info->rect.x, info->rect.y, info->rect.width, info->rect.height);
                break;
            }
            qDebug() << "invalid pointer of shm!";
            image_data = nullptr;
        }

        if (!image_data || qimage.isNull())
        {
            // get window image from XGetImage(a little slow)
            qDebug() << "get Image from dxcbplugin SHM failed!";
            qDebug() << "get Image from Xlib...";
            ximage = getImageXlib();
            if (!ximage)
            {
                qDebug() << "get Image from Xlib failed! giving up...";
                emit requestCheckWindow();
                return;
            }
            qimage = QImage((const uchar*)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);
        }

        Q_ASSERT(!qimage.isNull());

        // remove shadow frame
        m_snapshotSrcRect = rectRemovedShadow(qimage, prop_to_return_gtk);
        m_snapshot = qimage;
    } while (false);

    QSizeF size(rect().marginsRemoved(QMargins(8, 8, 8, 8)).size());
    const auto ratio = devicePixelRatioF();
    size = m_snapshotSrcRect.size().scaled(size * ratio, Qt::KeepAspectRatio);
    qreal scale = qreal(size.width()) / m_snapshotSrcRect.width();
    m_snapshot = m_snapshot.scaled(qRound(m_snapshot.width() * scale), qRound(m_snapshot.height() * scale),
                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_snapshotSrcRect.moveTop(m_snapshotSrcRect.top() * scale + 0.5);
    m_snapshotSrcRect.moveLeft(m_snapshotSrcRect.left() * scale + 0.5);
    m_snapshotSrcRect.setWidth(size.width() - 0.5);
    m_snapshotSrcRect.setHeight(size.height() - 0.5);
    m_snapshot.setDevicePixelRatio(ratio);

    if (image_data) shmdt(image_data);
    if (ximage) XDestroyImage(ximage);
    if (info) XFree(info);
    if (prop_to_return_gtk) XFree(prop_to_return_gtk);

    update();
}

void AppSnapshot::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);

    if (!windowPreviewsAvailable(m_wmHelper)) {
        m_closeBtn2D->setVisible(true);
    }
    else {
        emit entered(wid());
    }

    update();
}

void AppSnapshot::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_closeBtn2D->setVisible(false);

    update();
}

void AppSnapshot::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);

    if (!windowPreviewsAvailable(m_wmHelper))
    {
        if (underMouse())
            painter.fillRect(rect(), QColor(255, 255, 255, 255 * .2));
        return;
    }

    if (m_snapshot.isNull())
        return;

    const auto ratio = devicePixelRatioF();

    // draw attention background
    if (m_windowInfo.attention)
    {
        painter.setBrush(QColor(241, 138, 46, 255 * .8));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect(), 5, 5);
    }

    // draw image
    const QImage &im = m_snapshot;

    const qreal offset_x = width() / 2.0 - m_snapshotSrcRect.width() / ratio / 2;
    const qreal offset_y = height() / 2.0 - m_snapshotSrcRect.height() / ratio / 2;
    painter.drawImage(QPointF(offset_x, offset_y), im, m_snapshotSrcRect);
}

void AppSnapshot::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    QTimer::singleShot(1, this, &AppSnapshot::fetchSnapshot);
}

void AppSnapshot::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);

    emit clicked(m_wid);
}

SHMInfo * AppSnapshot::getImageDSHM()
{
    const auto display = x11Display();

    Atom atom_prop = XInternAtom(display, "_DEEPIN_DXCB_SHM_INFO", true);
    if (!atom_prop) {
        return nullptr;
    }

    Atom actual_type_return_deepin_shm;
    int actual_format_return_deepin_shm;
    unsigned long nitems_return_deepin_shm;
    unsigned long bytes_after_return_deepin_shm;
    unsigned char *prop_return_deepin_shm;

    XGetWindowProperty(display, m_wid, atom_prop, 0, 32 * 9, false, AnyPropertyType,
            &actual_type_return_deepin_shm, &actual_format_return_deepin_shm, &nitems_return_deepin_shm,
            &bytes_after_return_deepin_shm, &prop_return_deepin_shm);

    //qDebug() << actual_type_return_deepin_shm << actual_format_return_deepin_shm << nitems_return_deepin_shm << bytes_after_return_deepin_shm << prop_return_deepin_shm;

    return reinterpret_cast<SHMInfo *>(prop_return_deepin_shm);
}

XImage *AppSnapshot::getImageXlib()
{
    const auto display = x11Display();
    Window unused_window;
    int unused_int;
    unsigned unused_uint, w, h;
    XGetGeometry(display, m_wid, &unused_window, &unused_int, &unused_int, &w, &h, &unused_uint, &unused_uint);
    return XGetImage(display, m_wid, 0, 0, w, h, AllPlanes, ZPixmap);
}

QRect AppSnapshot::rectRemovedShadow(const QImage &qimage, unsigned char *prop_to_return_gtk)
{
    const auto display = x11Display();

    const Atom gtk_frame_extents = XInternAtom(display, "_GTK_FRAME_EXTENTS", true);
    Atom actual_type_return_gtk;
    int actual_format_return_gtk;
    unsigned long n_items_return_gtk;
    unsigned long bytes_after_return_gtk;

    const auto r = XGetWindowProperty(display, m_wid, gtk_frame_extents, 0, 4, false, XA_CARDINAL,
                                      &actual_type_return_gtk, &actual_format_return_gtk, &n_items_return_gtk, &bytes_after_return_gtk, &prop_to_return_gtk);
    if (!r && prop_to_return_gtk && n_items_return_gtk == 4 && actual_format_return_gtk == 32)
    {
        qDebug() << "remove shadow frame...";
        const unsigned long *extents = reinterpret_cast<const unsigned long *>(prop_to_return_gtk);
        const int left = extents[0];
        const int right = extents[1];
        const int top = extents[2];
        const int bottom = extents[3];
        const int width = qimage.width();
        const int height = qimage.height();

        return QRect(left, top, width - left - right, height - top - bottom);
    } else {
        return QRect(0, 0, qimage.width(), qimage.height());
    }
}
