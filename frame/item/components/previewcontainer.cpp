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

#include "previewcontainer.h"
#include "previewclosebutton.h"

#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QDragEnterEvent>
#include <QGuiApplication>

#define SPACING           0
#define MARGIN            0
#define SNAP_HEIGHT_WITHOUT_COMPOSITE       30

#define V23_TITLE_HEIGHT    24
#define V23_TITLE_INSET     10
#define V23_TITLE_TOP       8
#define V23_ICON_SPACING    6
#define V23_LIST_SPACING    2
static const QMargins V23ListMargins(8, V23_TITLE_TOP + V23_TITLE_HEIGHT + 4, 8, 8);

static bool windowPreviewsAvailable(DWindowManagerHelper *helper)
{
    return helper->hasComposite()
        || QGuiApplication::platformName().contains(
            "wayland", Qt::CaseInsensitive);
}

PreviewContainer::PreviewContainer(QWidget *parent)
    : QWidget(parent),
      m_needActivate(false),

      m_floatingPreview(new FloatingPreview(this)),
      m_mouseLeaveTimer(new QTimer(this)),
      m_wmHelper(DWindowManagerHelper::instance()),
      m_currentWId(0),
      m_closeAllButton(new PreviewCloseButton(this))
{
    m_windowListLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_windowListLayout->setSpacing(SPACING);
    m_windowListLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);

    m_mouseLeaveTimer->setSingleShot(true);
    m_mouseLeaveTimer->setInterval(300);

    m_floatingPreview->setVisible(false);

    m_waitForShowPreviewTimer = new QTimer(this);
    m_waitForShowPreviewTimer->setSingleShot(true);
    m_waitForShowPreviewTimer->setInterval(200);

    setAcceptDrops(true);
    setLayout(m_windowListLayout);
    setFixedSize(SNAP_WIDTH, SNAP_HEIGHT);

    m_closeAllButton->setVisible(false);

    connect(m_mouseLeaveTimer, &QTimer::timeout, this, &PreviewContainer::checkMouseLeave, Qt::QueuedConnection);
    connect(m_waitForShowPreviewTimer, &QTimer::timeout, this, &PreviewContainer::previewFloating);
    connect(m_closeAllButton, &PreviewCloseButton::clicked, this, &PreviewContainer::closeAllWindows);
}

void PreviewContainer::setAppIcon(const QPixmap &icon)
{
    m_appIcon = icon;
    update();
}

void PreviewContainer::setWindowInfos(const WindowInfoMap &infos, const WindowList &allowClose,
        const QString &appId)
{
    // check removed window
    for (auto it(m_snapshots.begin()); it != m_snapshots.end();)
    {
        if (!infos.contains(it.key()))
        {
            m_windowListLayout->removeWidget(it.value());
            it.value()->deleteLater();
            it = m_snapshots.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it(infos.cbegin()); it != infos.cend(); ++it)
    {
        const WId key = it.key();
        if (!m_snapshots.contains(key))
            appendSnapWidget(key);
        m_snapshots[key]->setWindowInfo(it.value());
        m_snapshots[key]->setCloseAble(allowClose.contains(key));
        m_snapshots[key]->setAppId(appId);
    }

    if (m_snapshots.isEmpty()) {
        emit requestCancelPreviewWindow();
        emit requestHidePopup();
    }

    if (!infos.isEmpty()) {
        m_title = infos.cbegin().value().title;
    }

    updateV23Layout();
    adjustSize();
}

void PreviewContainer::updateV23Layout()
{
    const bool v23 = Dock::isV23Mode() && windowPreviewsAvailable(m_wmHelper);
    if (v23) {
        m_windowListLayout->setSpacing(V23_LIST_SPACING);
        m_windowListLayout->setContentsMargins(V23ListMargins);
    } else {
        m_windowListLayout->setSpacing(SPACING);
        m_windowListLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    }

    bool closeAble = false;
    for (AppSnapshot *snap : m_snapshots)
        closeAble |= snap->closeAble();
    m_closeAllButton->setVisible(v23 && closeAble);
}

void PreviewContainer::closeAllWindows()
{
    for (AppSnapshot *snap : m_snapshots)
        if (snap->closeAble())
            snap->closeWindow();
}

void PreviewContainer::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    if (!Dock::isV23Mode() || !windowPreviewsAvailable(m_wmHelper)) {
        return;
    }

    // title row
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect iconRect(V23_TITLE_INSET, V23_TITLE_TOP, V23_TITLE_HEIGHT, V23_TITLE_HEIGHT);
    if (!m_appIcon.isNull())
        painter.drawPixmap(iconRect, m_appIcon);

    QRect textRect(iconRect.right() + 1 + V23_ICON_SPACING, V23_TITLE_TOP, 0, V23_TITLE_HEIGHT);
    textRect.setRight(m_closeAllButton->isVisibleTo(this) ? m_closeAllButton->x() - V23_ICON_SPACING
        : width() - V23_TITLE_INSET);
    painter.setPen(Dock::isV23Light() ? Qt::black : Qt::white);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
        fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width()));
}

void PreviewContainer::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    m_closeAllButton->move(width() - V23_TITLE_INSET - m_closeAllButton->width(), V23_TITLE_TOP);
}

void PreviewContainer::updateSnapshots()
{
    for (AppSnapshot *snap : m_snapshots)
        snap->fetchSnapshot();
}

void PreviewContainer::updateLayoutDirection(const Dock::Position dockPos)
{
    if (windowPreviewsAvailable(m_wmHelper)
            && (dockPos == Dock::Top || dockPos == Dock::Bottom))
        m_windowListLayout->setDirection(QBoxLayout::LeftToRight);
    else
        m_windowListLayout->setDirection(QBoxLayout::TopToBottom);

    updateV23Layout();
    adjustSize();
}

void PreviewContainer::checkMouseLeave()
{
    const bool hover = underMouse();

    if (hover)
        return;

    m_floatingPreview->setVisible(false);

    if (windowPreviewsAvailable(m_wmHelper)) {
        if (m_needActivate) {
            m_needActivate = false;
            emit requestActivateWindow(m_floatingPreview->trackedWid());
        } else {
            emit requestCancelPreviewWindow();
        }
    }

    emit requestHidePopup();
}

void PreviewContainer::prepareHide()
{
    m_mouseLeaveTimer->start();
}

void PreviewContainer::adjustSize()
{
    const int count = m_snapshots.size();
    const bool composite = windowPreviewsAvailable(m_wmHelper);
    if (!composite)
    {
        const int h = SNAP_HEIGHT_WITHOUT_COMPOSITE * count + MARGIN * 2 + SPACING * (count - 1);
        setFixedSize(SNAP_WIDTH, h);
        return;
    }

    QScreen *targetScreen = screen();
    if (!targetScreen)
        targetScreen = qApp->primaryScreen();
    const QRect r = targetScreen->geometry();
    const int padding = 20;

    const bool horizontal = m_windowListLayout->direction() == QBoxLayout::LeftToRight;
    if (Dock::isV23Mode())
    {
        QSize list;
        for (AppSnapshot *snap : m_snapshots) {
            const QSize card = snap->minimumSize();
            if (horizontal) {
                list.rwidth() += card.width();
                list.setHeight(std::max(list.height(), card.height()));
            } else {
                list.rheight() += card.height();
                list.setWidth(std::max(list.width(), card.width()));
            }
        }

        if (horizontal) {
            list.rwidth() += V23_LIST_SPACING * (count - 1);
        } else {
            list.rheight() += V23_LIST_SPACING * (count - 1);
        }

        const QMargins m = m_windowListLayout->contentsMargins();
        setFixedSize(std::min(list.width() + m.left() + m.right(), r.width() - padding),
            std::min(list.height() + m.top() + m.bottom(), r.height() - padding));
        return;
    }

    if (horizontal)
    {
        const int h = SNAP_HEIGHT + MARGIN * 2;
        const int w = SNAP_WIDTH * count + MARGIN * 2 + SPACING * (count - 1);

        setFixedHeight(h);
        setFixedWidth(std::min(w, r.width() - padding));
    } else {
        const int w = SNAP_WIDTH + MARGIN * 2;
        const int h = SNAP_HEIGHT * count + MARGIN * 2 + SPACING * (count - 1);

        setFixedWidth(w);
        setFixedHeight(std::min(h, r.height() - padding));
    }
}

void PreviewContainer::appendSnapWidget(const WId wid)
{
    AppSnapshot *snap = new AppSnapshot(wid);

    connect(snap, &AppSnapshot::clicked, this, &PreviewContainer::onSnapshotClicked, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::entered, this, &PreviewContainer::previewEntered, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::requestCheckWindow, this, &PreviewContainer::requestCheckWindows, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::sizeChanged, this, &PreviewContainer::adjustSize, Qt::QueuedConnection);

    m_windowListLayout->addWidget(snap);

    m_snapshots.insert(wid, snap);

    // refresh if visible
    if (isVisible())
        snap->fetchSnapshot();
}

void PreviewContainer::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);

    m_needActivate = false;
    m_mouseLeaveTimer->stop();

    if (windowPreviewsAvailable(m_wmHelper)) {
        m_waitForShowPreviewTimer->start();
    }
}

void PreviewContainer::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_mouseLeaveTimer->start();
    m_waitForShowPreviewTimer->stop();
}

void PreviewContainer::dragEnterEvent(QDragEnterEvent *e)
{
    if (!windowPreviewsAvailable(m_wmHelper))
        return;

    e->accept();

    m_needActivate = false;
    m_mouseLeaveTimer->stop();
}

void PreviewContainer::dragLeaveEvent(QDragLeaveEvent *e)
{
    e->ignore();

    m_needActivate = true;
    m_mouseLeaveTimer->start();
}

void PreviewContainer::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    adjustSize();
    m_windowListLayout->activate();
    QTimer::singleShot(0, this, [this] {
        if (isVisible())
            updateSnapshots();
    });
}

void PreviewContainer::onSnapshotClicked(const WId wid)
{
    const bool nativeWayland = QGuiApplication::platformName().contains(
        "wayland", Qt::CaseInsensitive);
    if (nativeWayland || !windowPreviewsAvailable(m_wmHelper)) {
        emit requestActivateWindow(wid);
    }

    m_needActivate = !nativeWayland;
    // the leaveEvent of this widget will be called after this signal
    Q_EMIT requestHidePopup();
}

void PreviewContainer::previewEntered(const WId wid)
{
    if (!windowPreviewsAvailable(m_wmHelper))
        return;

    AppSnapshot *snap = static_cast<AppSnapshot *>(sender());
    if (!snap) {
        return;
    }
    snap->setContentsMargins(100, 0, 100, 0);

    AppSnapshot *preSnap = m_floatingPreview->trackedWindow();
    if (preSnap && preSnap != snap) {
        preSnap->setContentsMargins(0, 0, 0, 0);
    }

    m_currentWId = wid;

    m_floatingPreview->trackWindow(snap);

    if (Dock::isV23Mode()) {
        m_title = snap->title();
        update();
        m_floatingPreview->setVisible(true);
        m_floatingPreview->raise();
    }

    if (m_waitForShowPreviewTimer->isActive()) {
        return;
    }

    previewFloating();
}

void PreviewContainer::previewFloating()
{
    if (!m_floatingPreview->trackedWindow())
        return;

    m_floatingPreview->setVisible(true);
    m_floatingPreview->raise();

    emit requestPreviewWindow(m_currentWId);
}
