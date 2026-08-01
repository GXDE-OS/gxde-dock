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

#include "dockpopupwindow.h"

#include <DApplication> 
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>

DWIDGET_USE_NAMESPACE

DockPopupWindow::DockPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent),
      m_model(false),

      m_acceptDelayTimer(new QTimer(this)),

      m_regionInter(new DRegionMonitor(this)),

      m_maskWindow(new DockPopupMask(this, nullptr))
{
    m_acceptDelayTimer->setSingleShot(true);
    m_acceptDelayTimer->setInterval(100);

    m_wmHelper = DWindowManagerHelper::instance();

    compositeChanged();

    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        // Manually set mask color on Wayland
        setBackgroundColor(QColor(36, 36, 36, 220));
    } else {
        setBackgroundColor(DBlurEffectWidget::DarkColor);
    }

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setProperty("_d_dock_popup", true);

    connect(m_acceptDelayTimer, &QTimer::timeout, this, &DockPopupWindow::accept);
    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &DockPopupWindow::compositeChanged);
    connect(m_regionInter, &DRegionMonitor::buttonPress, this, &DockPopupWindow::onGlobMouseRelease);
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return isVisible() && m_model;
}

void DockPopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;
    m_lastPoint = pos;

    show(pos.x(), pos.y());

    const bool nativeWayland = QGuiApplication::platformName().contains(
        "wayland", Qt::CaseInsensitive);
    if (!nativeWayland && m_regionInter->registered()) {
        m_regionInter->unregisterRegion();
    }

    if (!nativeWayland) {
        m_regionInter->registerRegion();
    }else{
        // Wayland 下显示遮罩窗口
        QScreen *screen = QGuiApplication::screenAt(pos);
        if (!screen) screen = QGuiApplication::primaryScreen();
        m_maskWindow->showMask(screen);
    }
}

void DockPopupWindow::show(const int x, const int y)
{
    m_lastPoint = QPoint(x, y);

    // Replace Qt::move() on Wayland by manually calculating position.
    const QSize popupSize = getFixedSize();
    QPoint topLeft;
    switch (arrowDirection()) {
    case ArrowTop:
        topLeft = QPoint(x - popupSize.width() / 2, y);
        break;
    case ArrowBottom:
        topLeft = QPoint(x - popupSize.width() / 2, y - popupSize.height());
        break;
    case ArrowLeft:
        topLeft = QPoint(x, y - popupSize.height() / 2);
        break;
    case ArrowRight:
        topLeft = QPoint(x - popupSize.width(), y - popupSize.height() / 2);
        break;
    }
    setProperty("_d_dock_popup_position", topLeft);

    DArrowRectangle::show(x, y);
}

void DockPopupWindow::hide()
{
    const bool nativeWayland = QGuiApplication::platformName().contains(
        "wayland", Qt::CaseInsensitive);
    if (!nativeWayland && m_regionInter->registered())
        m_regionInter->unregisterRegion();

    if (nativeWayland) {
        m_maskWindow->hideMask();
    }

    DArrowRectangle::hide();
}

void DockPopupWindow::showEvent(QShowEvent *e)
{
    DArrowRectangle::showEvent(e);

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

void DockPopupWindow::enterEvent(QEnterEvent *e)
{
    DArrowRectangle::enterEvent(e);

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize) {
        return false;
    }
    // FIXME: ensure position move after global mouse release event
    if (isVisible())
    {
        QTimer::singleShot(10, this, [=] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint, m_model);
        });
    }

    return false;
}

void DockPopupWindow::onGlobMouseRelease(const QPoint &mousePos, const int flag)
{

    if (!((flag == DRegionMonitor::WatchedFlags::Button_Left) ||
          (flag == DRegionMonitor::WatchedFlags::Button_Right))) {
        return;
    }

    if (this->frameGeometry().contains(mousePos))
        return;

    emit accept();

    if (m_regionInter->registered()) {
        m_regionInter->unregisterRegion();
    }
}

void DockPopupWindow::compositeChanged()
{
    if (m_wmHelper->hasComposite())
        setBorderColor(QColor(255, 255, 255, 255 * 0.05));
    else
        setBorderColor(QColor("#2C3238"));
}

void DockPopupWindow::ensureRaised()
{
    if (isVisible())
        raise();
}


DockPopupMask::DockPopupMask(DockPopupWindow *popup, QWidget *parent)
    : QWidget(parent), m_popup(popup)
{
    setAttribute(Qt::WA_TranslucentBackground);
    // Window flags 由 setMenuMaskRole 统一设置
}

void DockPopupMask::showMask(QScreen *screen)
{
    if (!screen) screen = QGuiApplication::primaryScreen();
    
    // 先配置 LayerShell，再显示
    Wayland::LayerShellHelper::setMenuMaskRole(this);
    
    setGeometry(screen->geometry());
    show();
    raise();
}

void DockPopupMask::hideMask()
{
    hide();
}

void DockPopupMask::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_popup && m_popup->isVisible()) {
        m_popup->hide();
        emit m_popup->accept();
    }
    hideMask();
}
