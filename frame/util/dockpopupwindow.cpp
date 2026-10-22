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
#include "constants.h"
#include "wayland/layershellhelper.h"
#include "wayland/layershell_styler.h"

#include <DApplication> 
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

DWIDGET_USE_NAMESPACE

QT_BEGIN_NAMESPACE
void qt_blurImage(QPainter *painter, QImage &image, qreal radius, bool quality,
                  bool alphaOnly, int transposed = 0);
QT_END_NAMESPACE

namespace {
constexpr int kV23ShadowMargin = 10;
constexpr int kV23ShadowBlur = 6;
constexpr int kV23DockGap = 10;
constexpr int kMarkPointGap = 2;
}

DockPopupWindow::DockPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent),
      m_model(false),
      m_v23(false),
      m_v23ShadowOffset(2),

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

    updateV23Style(content);
    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;
    m_lastPoint = pos;

    QScreen *screen = this->screen();
    if (!screen || !screen->geometry().contains(pos))
        screen = QGuiApplication::screenAt(pos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen && this->screen() != screen)
        setScreen(screen);

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
        m_maskWindow->showMask(screen);
        this->raise(); 
    }
}

void DockPopupWindow::show(int x, int y)
{
    m_lastPoint = QPoint(x, y);

    if (m_v23) {
        const int shift = kV23ShadowMargin + kMarkPointGap - kV23DockGap;
        switch (arrowDirection()) {
        case ArrowTop:      y -= shift; break;
        case ArrowBottom:   y += shift; break;
        case ArrowLeft:     x -= shift; break;
        case ArrowRight:    x += shift; break;
        }
    }

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

    if (m_v23) {
        applyV23Blur();
    }

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

void DockPopupWindow::paintEvent(QPaintEvent *e)
{
    if (!m_v23) {
        DArrowRectangle::paintEvent(e);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QRectF panel = v23PanelRect();
    QPainterPath path;
    path.addRoundedRect(panel, radius(), radius());

    const bool light = Dock::isV23Light();
    if (m_shadowCache.isNull() || m_shadowCacheSize != size()) {
        QImage shadow(size(), QImage::Format_ARGB32_Premultiplied);
        shadow.fill(Qt::transparent);
        {
            QPainter shadowPainter(&shadow);
            shadowPainter.setRenderHint(QPainter::Antialiasing, true);
            shadowPainter.fillPath(path.translated(0, m_v23ShadowOffset), Qt::black);
        }

        m_shadowCache = QImage(size(), QImage::Format_ARGB32_Premultiplied);
        m_shadowCache.fill(Qt::transparent);
        QPainter cachePainter(&m_shadowCache);
        qt_blurImage(&cachePainter, shadow, kV23ShadowBlur * 2.0, true, true);
        m_shadowCacheSize = size();
    }

    QPainterPath outside;
    outside.addRect(rect());
    outside = outside.subtracted(path);
    painter.save();
    painter.setClipPath(outside);
    painter.setOpacity(light ? 0.2 : 0.5);
    painter.drawImage(0, 0, m_shadowCache);
    painter.restore();

    // dde-shell PanelPopupWindow blend color over the compositor blur
    painter.fillPath(path, light ? QColor(235, 235, 235, 255 * 0.6) : QColor(0, 0, 0, 85));

    painter.setPen(QPen(light ? QColor(0, 0, 0, 255 * 0.1) : QColor(0, 0, 0, 255 * 0.5), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(panel.adjusted(.5, .5, -.5, -.5), radius() - .5, radius() - .5);
}

void DockPopupWindow::resizeEvent(QResizeEvent *e)
{
    DArrowRectangle::resizeEvent(e);

    if (m_v23 && isVisible()) {
        applyV23Blur();
    }
}

void DockPopupWindow::updateV23Style(QWidget *content)
{
    const bool v23 = Dock::isV23Mode() && Wayland::LayerShellHelper::isWayland();
    if (!v23 && !m_v23) {
        return;
    }

    m_v23 = v23;
    m_shadowCache = QImage();

    if (!v23) {
        if (QWindow *window = windowHandle())
            window->setMask(QRegion());

        setShadowBlurRadius(0);
        setShadowYOffset(0);
        setArrowWidth(18);
        setArrowHeight(10);
        setMargin(8);
        setRadius(6);
        return;
    }

    const bool preview = content->inherits("PreviewContainer");
    const bool padded = preview || content->inherits("TipsWidget");

    setArrowWidth(0);
    setArrowHeight(0);
    setShadowBlurRadius(kV23ShadowMargin);
    setShadowYOffset(0);
    setMargin(padded ? 0 : 8);
    setRadius(preview ? 6 : 8);
    m_v23ShadowOffset = preview ? 4 : 2;
}

QRect DockPopupWindow::v23PanelRect() const
{
    return rect().marginsRemoved(QMargins(kV23ShadowMargin, kV23ShadowMargin,
        kV23ShadowMargin, kV23ShadowMargin));
}

void DockPopupWindow::applyV23Blur()
{
    QWindow *window = windowHandle();
    if (!window) {
        return;
    }

    window->setMask(QRegion(v23PanelRect()));
    Wayland::LayerShellStyler::apply(window, radius(), true, v23PanelRect());
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
    
    if (Wayland::LayerShellHelper::isWayland()) {
        // Wayland: 使用 layer shell 确保遮罩渲染在 dock 的 layer shell 窗口之上，
        // 这样才能拦截 dock 区域内的点击事件。
        setScreen(screen);
        Wayland::LayerShellHelper::setMenuMaskRole(this);
        show();
    } else {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_X11DoNotAcceptFocus, true);

        setGeometry(screen->geometry());
        show();
    }
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
