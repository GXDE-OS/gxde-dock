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

#include "launcheritem.h"
#include "util/docksettings.h"
#include "util/themeappicon.h"
#include "util/imagefactory.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <DDBusSender>
#include <QApplication>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QScreen>
#include <QWindow>

DCORE_USE_NAMESPACE

LauncherItem::LauncherItem(QWidget *parent)
    : DockItem(parent)
    , m_launcherInter(new LauncherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
    , m_tips(new TipsWidget(this))
{
    m_launcherInter->setSync(true, false);

    setAccessibleName("Launcher");
    m_tips->setVisible(false);
    m_tips->setObjectName("launcher");
}

void LauncherItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    if (DockDisplayMode == Efficient)
    {
        m_smallIcon = ThemeAppIcon::getIcon("deepin-launcher", iconSize * 0.7, devicePixelRatioF());
        m_largeIcon = ThemeAppIcon::getIcon("deepin-launcher", iconSize * 0.9, devicePixelRatioF());
    } else {
        m_smallIcon = ThemeAppIcon::getIcon("deepin-launcher", iconSize * 0.6, devicePixelRatioF());
        m_largeIcon = ThemeAppIcon::getIcon("deepin-launcher", iconSize * 0.8, devicePixelRatioF());
    }

    update();
}

void LauncherItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (!isVisible())
        return;

    QPainter painter(this);

    const QPixmap pixmap = DockDisplayMode == Fashion ? m_largeIcon : m_smallIcon;

    // 居中偏移必须用 pixmap 自身的 DPR（修复 HDPI 下图标偏左上）
    const int iconX = rect().center().x() - pixmap.rect().center().x() / pixmap.devicePixelRatioF();
    const int iconY = rect().center().y() - pixmap.rect().center().y() / pixmap.devicePixelRatioF();

    painter.drawPixmap(iconX, iconY, pixmap);
}

void LauncherItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void LauncherItem::mousePressEvent(QMouseEvent *e)
{
    hidePopup();

    return QWidget::mousePressEvent(e);
}

void LauncherItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    if (!m_launcherInter->IsVisible()) {
        QScreen *screen = nullptr;
        if (QWidget *topLevel = window()) {
            if (QWindow *handle = topLevel->windowHandle())
                screen = handle->screen();
            if (!screen)
                screen = topLevel->screen();
        }
        if (!screen)
            screen = qApp->primaryScreen();

        if (!screen) {
            m_launcherInter->Show();
            return;
        }

        const QRect dockGeometry =
            DockSettings::Instance().frontendWindowRect(screen);
        QDBusPendingCall call = m_launcherInter->asyncCall(
            QStringLiteral("ShowOnScreen"),
            screen->name(), dockGeometry.x(), dockGeometry.y(),
            static_cast<uint>(dockGeometry.width()),
            static_cast<uint>(dockGeometry.height()));
        auto *watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher] {
            if (watcher->isError()) {
                qInfo() << "Launcher.ShowOnScreen unavailable, falling back to Show:"
                        << watcher->error().name();
                m_launcherInter->Show();
            }
            watcher->deleteLater();
        });
    }
}

QWidget *LauncherItem::popupTips()
{
    m_tips->setText(tr("Launcher"));
    return m_tips;
}
