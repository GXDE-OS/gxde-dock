/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             listenerri <listenerri@gmail.com>
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

#include "appitem.h"

#include "util/themeappicon.h"
#include "util/imagefactory.h"
#include "xcb/xcb_misc.h"
#include "components/appswingeffectbuilder.h"
#include "components/appspreviewprovider.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#include <QPainter>
#include <QDrag>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <QTimeLine>
#include <QX11Info>

#define APP_DRAG_THRESHOLD      20

int AppItem::IconBaseSize;
QPoint AppItem::MousePressPos;

AppItem::AppItem(const QDBusObjectPath &entry, QWidget *parent)
    : DockItem(parent),
      m_appNameTips(new TipsWidget(this)),
      m_appPreviewTips(nullptr),
      m_itemEntryInter(new DockEntryInter("com.deepin.dde.daemon.Dock", entry.path(), QDBusConnection::sessionBus(), this)),

      m_swingEffectView(nullptr),
      m_itemAnimation(nullptr),

      m_wmHelper(DWindowManagerHelper::instance()),

      m_drag(nullptr),

      m_dragging(false),

      m_retryTimes(0),

      m_appIcon(QPixmap()),

      m_horizontalIndicator(QPixmap(":/indicator/resources/indicator.png")),
      m_verticalIndicator(QPixmap(":/indicator/resources/indicator_ver.png")),
      m_activeHorizontalIndicator(QPixmap(":/indicator/resources/indicator_active.png")),
      m_activeVerticalIndicator(QPixmap(":/indicator/resources/indicator_active_ver.png")),
      m_updateIconGeometryTimer(new QTimer(this)),
      m_retryObtainIconTimer(new QTimer(this)),

      m_smallWatcher(new QFutureWatcher<QPixmap>(this)),
      m_largeWatcher(new QFutureWatcher<QPixmap>(this))
{
    QHBoxLayout *centralLayout = new QHBoxLayout;
    centralLayout->setMargin(0);
    centralLayout->setSpacing(0);
    setStyleSheet("background-color: rgba(255, 255, 255, 0);");  // 任务栏启动程序时不会出现阴影

    setAccessibleName(m_itemEntryInter->name());
    setAcceptDrops(true);
    setLayout(centralLayout);

    m_id = m_itemEntryInter->id();
    m_active = m_itemEntryInter->isActive();

    m_appNameTips->setObjectName("AppItemTips");
    m_appNameTips->setAccessibleName(m_itemEntryInter->name() + "-tips");
    m_appNameTips->setVisible(false);
    m_appNameTips->installEventFilter(this);

    m_updateIconGeometryTimer->setInterval(500);
    m_updateIconGeometryTimer->setSingleShot(true);

    m_retryObtainIconTimer->setInterval(500);
    m_retryObtainIconTimer->setSingleShot(true);

    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, &AppItem::activeChanged);
    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, static_cast<void (AppItem::*)()>(&AppItem::update));
    connect(m_itemEntryInter, &DockEntryInter::WindowInfosChanged, this, &AppItem::updateWindowInfos, Qt::QueuedConnection);
    connect(m_itemEntryInter, &DockEntryInter::IconChanged, this, &AppItem::refershIcon);

    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, &AppItem::updateWindowIconGeometries, Qt::QueuedConnection);
    connect(m_retryObtainIconTimer, &QTimer::timeout, this, &AppItem::refershIcon, Qt::QueuedConnection);

    updateWindowInfos(m_itemEntryInter->windowInfos());
    refershIcon();
}

AppItem::~AppItem()
{
    stopSwingEffect();

    m_appNameTips->deleteLater();
}

const QString AppItem::appId() const
{
    return m_id;
}

const bool AppItem::isValid() const
{
    return m_itemEntryInter->isValid() && !m_itemEntryInter->id().isEmpty();
}

// Update _NET_WM_ICON_GEOMETRY property for windows that every item
// that manages, so that WM can do proper animations for specific
// window behaviors like minimization.
void AppItem::updateWindowIconGeometries()
{
    const QRect r(mapToGlobal(QPoint(0, 0)),
                  mapToGlobal(QPoint(width(),height())));
    auto *xcb_misc = XcbMisc::instance();

    for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); ++it)
        xcb_misc->set_window_icon_geometry(it.key(), r);
}

void AppItem::setIconBaseSize(const int size)
{
    IconBaseSize = size;
}

int AppItem::iconBaseSize()
{
    return IconBaseSize;
}

int AppItem::itemBaseWidth()
{
    if (DockDisplayMode == Dock::Fashion || DockDisplayMode == Dock::Classic)
        return itemBaseHeight() * 1.1;
    else
        return itemBaseHeight() * 1.4;
}

void AppItem::undock()
{
    m_itemEntryInter->RequestUndock();
}

QWidget *AppItem::appDragWidget()
{
    if (m_drag) {
        return m_drag->appDragWidget();
    }

    return nullptr;
}

void AppItem::setDockInfo(Dock::Position dockPosition, const QRect &dockGeometry)
{
    if (m_drag) {
        m_drag->appDragWidget()->setDockInfo(dockPosition, dockGeometry);
    }
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

    if (m_drag) {
        m_drag->appDragWidget()->setOriginPos(mapToGlobal(appIconPosition()));
    }

    m_updateIconGeometryTimer->start();
}

int AppItem::itemBaseHeight()
{
    if (DockDisplayMode == Efficient || DockDisplayMode == Classic)
        return IconBaseSize * 1.2;
    else
        return IconBaseSize * 1.5;
}

void AppItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (m_dragging || (m_swingEffectView != nullptr && DockDisplayMode != Fashion))
        return;

    QPainter painter(this);
    if (!painter.isActive())
        return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF itemRect = rect();

    // draw background
    QRectF backgroundRect = itemRect;

    if (DockDisplayMode == Efficient)
    {
        backgroundRect = itemRect.marginsRemoved(QMargins(1, 1, 1, 1));

        if (m_active)
        {
            painter.fillRect(backgroundRect, QColor(44, 167, 248, 255 * 0.3));

            const int activeLineWidth = itemRect.height() > 50 ? 4 : 2;
            QRectF activeRect = backgroundRect;
            switch (DockPosition)
            {
            case Top:       activeRect.setBottom(activeRect.top() + activeLineWidth);   break;
            case Bottom:    activeRect.setTop(activeRect.bottom() - activeLineWidth);   break;
            case Left:      activeRect.setRight(activeRect.left() + activeLineWidth);   break;
            case Right:     activeRect.setLeft(activeRect.right() - activeLineWidth);   break;
            }

            painter.fillRect(activeRect, QColor(44, 167, 248, 255));
        }
        else if (!m_windowInfos.isEmpty())
        {
            if (hasAttention())
                painter.fillRect(backgroundRect, QColor(241, 138, 46, 255 * .8));
            else
                painter.fillRect(backgroundRect, QColor(255, 255, 255, 255 * 0.2));
        }
    }
    else
    {
        if (!m_windowInfos.isEmpty())
        {
            QPoint p;
            QPixmap pixmap;
            QPixmap activePixmap;
            switch (DockPosition)
            {
            case Top:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(1);
                break;
            case Bottom:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(itemRect.height() - pixmap.height() - 1);
                break;
            case Left:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            case Right:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(itemRect.width() - pixmap.width() - 1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            }

            if (m_active)
                painter.drawPixmap(p, activePixmap);
            else
                painter.drawPixmap(p, pixmap);
        }
    }

    if (m_swingEffectView != nullptr)
        return;

    // icon
    if (m_appIcon.isNull())
        return;

    painter.drawPixmap(appIconPosition(), m_appIcon);
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        m_itemEntryInter->NewInstance(QX11Info::getTimestamp());

        // play launch effect
        if (m_windowInfos.isEmpty())
            playSwingEffect();

    } else if (e->button() == Qt::LeftButton) {
        if (checkAndResetTapHoldGestureState() && e->source() == Qt::MouseEventSynthesizedByQt) {
            qDebug() << "tap and hold gesture detected, ignore the synthesized mouse release event";
            return;
        }

        qDebug() << "app item clicked, name:" << m_itemEntryInter->name()
            << "id:" << m_itemEntryInter->id() << "my-id:" << m_id << "icon:" << m_itemEntryInter->icon();

        m_itemEntryInter->Activate(QX11Info::getTimestamp());

        // play launch effect
        if (m_windowInfos.isEmpty())
            playSwingEffect();
    }
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    m_updateIconGeometryTimer->stop();
    hidePopup();

    if (e->button() == Qt::LeftButton)
        MousePressPos = e->pos();

    // context menu will handle in DockItem
    DockItem::mousePressEvent(e);
}

void AppItem::mouseMoveEvent(QMouseEvent *e)
{
    e->accept();

    // handle preview
//    if (e->buttons() == Qt::NoButton)
//        return showPreview();

    // handle drag
    if (e->buttons() != Qt::LeftButton)
        return;

    const QPoint pos = e->pos();
    if (!rect().contains(pos))
        return;

    const QPoint distance = pos - MousePressPos;
    if (distance.manhattanLength() > APP_DRAG_THRESHOLD)
        return startDrag();
}

void AppItem::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);

    if (qAbs(e->angleDelta().y()) > 20) {
        m_itemEntryInter->PresentWindows();
    }
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source()) {
        return e->ignore();
    }

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop") {
        return e->ignore();
    }

    e->accept();
}

void AppItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if (m_windowInfos.isEmpty())
        return;

    if (!PopupWindow->isVisible() || !m_appPreviewTips)
        showPreview();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls()) {
        uriList << uri.toEncoded();
    }

    qDebug() << "accept drop event with URIs: " << uriList;
    m_itemEntryInter->HandleDragDrop(QX11Info::getTimestamp(), uriList);
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    if (m_appPreviewTips) {
        if (m_appPreviewTips->isVisible()) {
            m_appPreviewTips->prepareHide();
        }
    }
}

void AppItem::showEvent(QShowEvent *e)
{
    DockItem::showEvent(e);

    refershIcon();
}

void AppItem::showHoverTips()
{
    if (!m_windowInfos.isEmpty())
        return showPreview();

    DockItem::showHoverTips();
}

void AppItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(checked);

    m_itemEntryInter->HandleMenuItem(QX11Info::getTimestamp(), itemId);
}

const QString AppItem::contextMenu() const
{
    return m_itemEntryInter->menu();
}

QWidget *AppItem::popupTips()
{
    if (m_dragging)
        return nullptr;

    if (!m_windowInfos.isEmpty())
    {
        const quint32 currentWindow = m_itemEntryInter->currentWindow();
        Q_ASSERT(m_windowInfos.contains(currentWindow));
        m_appNameTips->setText(m_windowInfos[currentWindow].title);
    } else {
        m_appNameTips->setText(m_itemEntryInter->name());
    }

    return m_appNameTips;
}

void AppItem::startDrag()
{
    m_dragging = true;
    update();

    const QPixmap &dragPix = m_appIcon;

    m_drag = new AppDrag(this);
    m_drag->setMimeData(new QMimeData);

    // handle drag finished here
    connect(m_drag->appDragWidget(), &AppDragWidget::destroyed, this, [=] {
        m_dragging = false;
        m_drag.clear();
        setVisible(true);
        update();
    });

    if (m_wmHelper->hasComposite()) {
        m_drag->setPixmap(dragPix);
        m_drag->appDragWidget()->setOriginPos(mapToGlobal(appIconPosition()));
        emit dragStarted();
        m_drag->exec(Qt::MoveAction);
    } else {
        m_drag->QDrag::setPixmap(dragPix);
        m_drag->setHotSpot(dragPix.rect().center() / dragPix.devicePixelRatioF());
        emit dragStarted();
        m_drag->QDrag::exec(Qt::MoveAction);
    }

    // MainPanel will put this item to Item-Container when received this signal(MainPanel::itemDropped)
    //emit itemDropped(m_drag->target());

    if (!m_wmHelper->hasComposite()) {
        if (!m_drag->target()) {
            m_itemEntryInter->RequestUndock();
        }
    }
}

bool AppItem::hasAttention() const
{
    for (const auto &info : m_windowInfos)
        if (info.attention)
            return true;
    return false;
}

QPoint AppItem::appIconPosition() const
{
    const auto ratio = devicePixelRatioF();
    const QRectF itemRect = rect();
    const QRectF iconRect = m_appIcon.rect();
    const qreal iconX = itemRect.center().x() - iconRect.center().x() / ratio;
    const qreal iconY = itemRect.center().y() - iconRect.center().y() / ratio;

    return QPoint(iconX, iconY);
}

void AppItem::updateWindowInfos(const WindowInfoMap &info)
{
    m_windowInfos = info;
    if (m_appPreviewTips) m_appPreviewTips->setWindowInfos(m_windowInfos, m_itemEntryInter->GetAllowedCloseWindows().value());
    m_updateIconGeometryTimer->start();

    // process attention effect
    if (hasAttention())
    {
        if (DockDisplayMode == DisplayMode::Fashion || DockDisplayMode == DisplayMode::Classic)
            playSwingEffect();
    } else {
        stopSwingEffect();
    }

    update();
}

void AppItem::refershIcon()
{
    if (!isVisible())
        return;

    const QString icon = m_itemEntryInter->icon();
    const int iconSize = qMin(width(), height());

    if (DockDisplayMode == Efficient)
        m_appIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.7, devicePixelRatioF());
    else
        m_appIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.8, devicePixelRatioF());

    if (m_appIcon.isNull()) {
        if (m_retryTimes < 5) {
            m_retryTimes++;
            qDebug() << m_itemEntryInter->name() << "obtain app icon(" << icon << ")failed, retry times:" << m_retryTimes;
            m_retryObtainIconTimer->start();
        }
        return;
    } else if (m_retryTimes > 0) {
        // reset times
        m_retryTimes = 0;
    }

    update();

    m_updateIconGeometryTimer->start();
}

void AppItem::activeChanged()
{
    m_active = !m_active;
}

void AppItem::showPreview()
{
    if (m_windowInfos.isEmpty())
        return;

    m_appPreviewTips = PreviewWindow(m_windowInfos, m_itemEntryInter->GetAllowedCloseWindows().value(), DockPosition);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, this, &AppItem::requestCancelPreview);
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, &AppItem::hidePopup);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntryInter, &DockEntryInter::Check);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, [=]() { m_appPreviewTips = nullptr; });
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, [=]() { m_appPreviewTips = nullptr; });
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, [=]() { m_appPreviewTips = nullptr; });

    showPopupWindow(m_appPreviewTips, true);
}

void AppItem::playSwingEffect()
{
    // NOTE(sbw): return if animation view already playing
    if (m_swingEffectView != nullptr)
        return;

    stopSwingEffect();

    QPair<QGraphicsView *, QGraphicsItemAnimation *> pair =  SwingEffect(
            this, m_appIcon, rect(), devicePixelRatioF());

    m_swingEffectView = pair.first;
    m_itemAnimation = pair.second;

    QTimeLine *tl = m_itemAnimation->timeLine();
    connect(tl, &QTimeLine::stateChanged, [=](QTimeLine::State newState) {
        if (newState == QTimeLine::NotRunning) {
            m_swingEffectView->hide();
            layout()->removeWidget(m_swingEffectView);
            m_swingEffectView = nullptr;
            m_itemAnimation = nullptr;
            checkAttentionEffect();
        }
    });

    layout()->addWidget(m_swingEffectView);
    tl->start();
}

void AppItem::stopSwingEffect()
{
    if (m_swingEffectView == nullptr || m_itemAnimation == nullptr)
        return;

    // stop swing effect
    m_swingEffectView->setVisible(false);

    if (m_itemAnimation->timeLine() && m_itemAnimation->timeLine()->state() != QTimeLine::NotRunning)
        m_itemAnimation->timeLine()->stop();
}

void AppItem::checkAttentionEffect()
{
    QTimer::singleShot(1000, this, [=] {
        if ((DockDisplayMode == DisplayMode::Fashion || DockDisplayMode == DisplayMode::Classic) && hasAttention())
            playSwingEffect();
    });
}

