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
#include <DPlatformWindowHandle>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
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

// fadingpopups of deepin-kwin 5.27.2 on Deepin V23: a dock popup grows from
// 30% of its size at the dock side under the pointer while fading in, and
// fades out at its full size, both on an OutQuart curve. The blur cannot fade
// with it here, so a closing popup shrinks back instead, taking the blur along
constexpr int kOpenDuration = 150;
constexpr int kCloseDuration = 180;
constexpr qreal kOpenStartSize = 0.3;
}

// Covers the popup during its animation, and paints the popup as it looked
// when the animation started, moved, scaled and faded
class DockPopupAnimationOverlay : public QWidget
{
public:
    explicit DockPopupAnimationOverlay(DockPopupWindow *popup)
        : QWidget(popup)
        , m_popup(popup)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        m_popup->paintAnimation(painter);
    }

private:
    DockPopupWindow *m_popup;
};

DockPopupWindow::DockPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent),
      m_model(false),
      m_v23(false),
      m_v23ShadowOffset(2),
      m_classicShadowBlurRadius(0),
      m_classicShadowYOffset(0),
      m_classicBorderWidth(0),
      m_animation(Animation::None),
      m_animator(new QVariantAnimation(this)),
      m_animationOverlay(new DockPopupAnimationOverlay(this)),

      m_acceptDelayTimer(new QTimer(this)),

      m_regionInter(new DRegionMonitor(this)),

      m_maskWindow(new DockPopupMask(this, nullptr))
{
    m_acceptDelayTimer->setSingleShot(true);
    m_acceptDelayTimer->setInterval(100);

    m_wmHelper = DWindowManagerHelper::instance();

    m_classicShadowBlurRadius = shadowBlurRadius();
    m_classicShadowYOffset = shadowYOffset();
    m_classicBorderWidth = borderWidth();

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

    m_animator->setStartValue(0.0);
    m_animator->setEndValue(1.0);
    m_animator->setEasingCurve(QEasingCurve::OutQuart);
    connect(m_animator, &QVariantAnimation::valueChanged, this, [this] {
        m_animationOverlay->update();
        applyV23BlurArea(animatedPanelRect(m_animator->currentValue().toReal()).toAlignedRect());
    });
    connect(m_animator, &QVariantAnimation::finished, this, [this] {
        const Animation finished = m_animation;
        stopAnimation();
        if (finished == Animation::Closing)
            hideNow();
    });

    connect(m_acceptDelayTimer, &QTimer::timeout, this, &DockPopupWindow::accept);
    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &DockPopupWindow::compositeChanged);
    connect(m_regionInter, &DRegionMonitor::buttonPress, this, &DockPopupWindow::onGlobMouseRelease);
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return isVisible() && m_model && m_animation != Animation::Closing;
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
    // a popup opens when it was hidden or closing, or shows other content;
    // moving a shown one around is no opening
    const bool opening = model && m_v23 && v23Translucent()
        && (!isVisible() || m_animation == Animation::Closing || !m_model || getContent() != m_openedContent);
    if (m_animation == Animation::Closing)
        stopAnimation();

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

    if (opening) {
        m_openedContent = getContent();
        startAnimation(Animation::Opening);
    }
}

void DockPopupWindow::show(int x, int y)
{
    m_lastPoint = QPoint(x, y);

    if (m_v23) {
        const int shift = v23ShadowMargin() + kMarkPointGap - kV23DockGap;
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

    // dde-shell keeps popups 10px inside the screen
    // But we also have 10px margin. Recalculate the margin for good positioning.
    if (m_v23) {
        if (QScreen *screen = this->screen()) {
            const QRect bounds = screen->geometry();
            topLeft.setX(qBound(bounds.left(), topLeft.x(), bounds.right() + 1 - popupSize.width()));
            topLeft.setY(qBound(bounds.top(), topLeft.y(), bounds.bottom() + 1 - popupSize.height()));
        }
    }
    setProperty("_d_dock_popup_position", topLeft);

    DArrowRectangle::show(x, y);

    // on X11 the window is really moved, and kept on screen by dtk
    m_anchor = QPoint(x, y) - (Wayland::LayerShellHelper::isWayland() ? topLeft : pos());
}

void DockPopupWindow::hide()
{
    if (m_animation == Animation::Closing)
        return;

    // the mask goes at once, the clicks pass while the popup fades
    const bool nativeWayland = QGuiApplication::platformName().contains(
        "wayland", Qt::CaseInsensitive);
    if (!nativeWayland && m_regionInter->registered())
        m_regionInter->unregisterRegion();

    if (nativeWayland) {
        m_maskWindow->hideMask();
    }

    if (m_v23 && v23Translucent() && m_model && isVisible()) {
        startAnimation(Animation::Closing);
        return;
    }

    hideNow();
}

void DockPopupWindow::hideNow()
{
    // hidden first, so that stopping does not put the full blur back
    DArrowRectangle::hide();
    stopAnimation();
    m_openedContent = nullptr;
}

void DockPopupWindow::startAnimation(Animation animation)
{
    stopAnimation();

    // the popup as it is, without the overlay
    m_snapshot = grab();
    m_animation = animation;
    m_animator->setDuration(animation == Animation::Opening ? kOpenDuration : kCloseDuration);
    m_animationOverlay->setGeometry(rect());
    m_animationOverlay->raise();
    m_animationOverlay->show();
    // X11 maps the window at once, with the full blur set on show
    if (!Wayland::LayerShellHelper::isWayland())
        applyV23BlurArea(animatedPanelRect(0).toAlignedRect());
    m_animator->start();
}

void DockPopupWindow::stopAnimation()
{
    if (m_animation == Animation::None)
        return;

    m_animation = Animation::None;
    m_animator->stop();
    m_animationOverlay->hide();
    m_snapshot = QPixmap();
    if (isVisible())
        applyV23Blur();
}

QRectF DockPopupWindow::animatedPanelRect(qreal progress) const
{
    const QRectF panel = v23PanelRect();
    if (m_animation == Animation::None)
        return panel;
    if (m_animation == Animation::Closing)
        progress = 1.0 - progress;

    // from 30% of the size, centered on the pointer along the dock and 15%
    // of the size off the dock side: its edge then lies on the dock side
    const QSizeF startSize = panel.size() * kOpenStartSize;
    QPointF start(qBound(panel.left(), qreal(m_anchor.x()), panel.right()),
                  qBound(panel.top(), qreal(m_anchor.y()), panel.bottom()));
    switch (arrowDirection()) {
    case ArrowBottom: start.setY(panel.bottom() - startSize.height() / 2); break;
    case ArrowTop:    start.setY(panel.top() + startSize.height() / 2);    break;
    case ArrowRight:  start.setX(panel.right() - startSize.width() / 2);   break;
    case ArrowLeft:   start.setX(panel.left() + startSize.width() / 2);    break;
    }

    const QSizeF size = startSize + (panel.size() - startSize) * progress;
    const QPointF center = start + (panel.center() - start) * progress;
    return QRectF(center - QPointF(size.width() / 2, size.height() / 2), size);
}

void DockPopupWindow::paintAnimation(QPainter &painter)
{
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_snapshot.isNull())
        return;

    const qreal progress = m_animator->currentValue().toReal();
    const QRectF panel = v23PanelRect();
    const QRectF current = animatedPanelRect(progress);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setOpacity(m_animation == Animation::Opening ? progress : 1.0 - progress);
    painter.translate(current.topLeft());
    painter.scale(current.width() / panel.width(), current.height() / panel.height());
    painter.translate(-panel.topLeft());
    painter.drawPixmap(0, 0, m_snapshot);
}

void DockPopupWindow::showEvent(QShowEvent *e)
{
    DArrowRectangle::showEvent(e);

    if (m_v23 && m_animation == Animation::None) {
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
    const bool light = Dock::isV23Light();

    if (!v23Translucent()) {
        // X11 without a compositor shows no alpha: an opaque, square panel
        painter.fillRect(panel, light ? QColor(235, 235, 235) : QColor(36, 36, 36));
        painter.setPen(QPen(light ? QColor(0, 0, 0, 255 * 0.1) : QColor(0, 0, 0, 255 * 0.5), 1));
        painter.drawRect(panel.adjusted(.5, .5, -.5, -.5));
        return;
    }

    QPainterPath path;
    path.addRoundedRect(panel, radius(), radius());

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
    painter.fillPath(path, light ? QColor(235, 235, 235, 255 * 0.45) : QColor(0, 0, 0, 64));

    painter.setPen(QPen(light ? QColor(0, 0, 0, 255 * 0.1) : QColor(0, 0, 0, 255 * 0.5), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(panel.adjusted(.5, .5, -.5, -.5), radius() - .5, radius() - .5);
}

void DockPopupWindow::resizeEvent(QResizeEvent *e)
{
    DArrowRectangle::resizeEvent(e);

    // the window may get its size only after the animation started
    if (m_animation != Animation::None) {
        m_animationOverlay->hide();
        m_snapshot = grab();
        m_animationOverlay->setGeometry(rect());
        m_animationOverlay->show();
        return;
    }

    if (m_v23 && isVisible()) {
        applyV23Blur();
    }
}

void DockPopupWindow::updateV23Style(QWidget *content)
{
    const bool v23 = Dock::isV23Mode();
    if (!v23 && !m_v23) {
        return;
    }

    const bool wayland = Wayland::LayerShellHelper::isWayland();
    m_v23 = v23;
    m_shadowCache = QImage();

    if (!v23) {
        if (wayland) {
            if (QWindow *window = windowHandle())
                window->setMask(QRegion());

            setShadowBlurRadius(0);
            setShadowYOffset(0);
        } else {
            DPlatformWindowHandle::setWindowBlurAreaByWM(this, QVector<DPlatformWindowHandle::WMBlurArea>());
            setShadowBlurRadius(m_classicShadowBlurRadius);
            setShadowYOffset(m_classicShadowYOffset);
            setBorderWidth(m_classicBorderWidth);
            // brings back the blur widget of dtk, behind the whole window
            setBackgroundColor(QColor(0, 0, 0, 0));
            setBackgroundColor(DBlurEffectWidget::DarkColor);
        }
        setArrowWidth(18);
        setArrowHeight(10);
        setMargin(8);
        setRadius(6);
        return;
    }

    const bool preview = content->inherits("PreviewContainer");
    const bool padded = preview || content->inherits("TipsWidget");
    const int contentMargin = padded ? 0 : 8;

    setArrowWidth(0);
    setArrowHeight(0);
    setShadowYOffset(0);
    if (wayland) {
        setShadowBlurRadius(kV23ShadowMargin);
        setMargin(contentMargin);
    } else {
        // the dxcb window has no room for a shadow of ours, nor a frame we
        // want: the margin holds the shadow, which is painted like on Wayland
        setShadowBlurRadius(0);
        setBorderWidth(0);
        setMargin(contentMargin + v23ShadowMargin());
        // an opaque color drops the blur widget of dtk, that would blur the
        // shadow too; the panel alone is blurred instead
        setBackgroundColor(QColor(0, 0, 0));
    }
    setRadius(preview ? 6 : 8);
    m_v23ShadowOffset = preview ? 4 : 2;
}

bool DockPopupWindow::v23Translucent() const
{
    return Wayland::LayerShellHelper::isWayland() || m_wmHelper->hasComposite();
}

int DockPopupWindow::v23ShadowMargin() const
{
    return v23Translucent() ? kV23ShadowMargin : 0;
}

QRect DockPopupWindow::v23PanelRect() const
{
    const int margin = v23ShadowMargin();
    return rect().marginsRemoved(QMargins(margin, margin, margin, margin));
}

void DockPopupWindow::applyV23Blur()
{
    QWindow *window = windowHandle();
    if (!window) {
        return;
    }

    if (Wayland::LayerShellHelper::isWayland())
        window->setMask(QRegion(v23PanelRect()));
    applyV23BlurArea(v23PanelRect());
}

void DockPopupWindow::applyV23BlurArea(const QRect &panel)
{
    if (Wayland::LayerShellHelper::isWayland()) {
        if (QWindow *window = windowHandle())
            Wayland::LayerShellStyler::apply(window, radius(), true, panel);
        return;
    }

    if (!m_wmHelper->hasComposite() || !windowHandle())
        return;

    DPlatformWindowHandle::WMBlurArea area;
    area.x = panel.x();
    area.y = panel.y();
    area.width = panel.width();
    area.height = panel.height();
    area.xRadius = radius();
    area.yRaduis = radius();
    DPlatformWindowHandle::setWindowBlurAreaByWM(this, QVector<DPlatformWindowHandle::WMBlurArea>() << area);
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

    // the V23 panel gains or loses its shadow margin
    if (m_v23 && getContent()) {
        stopAnimation();
        setContent(getContent());
        if (isVisible())
            show(m_lastPoint, m_model);
    }
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
