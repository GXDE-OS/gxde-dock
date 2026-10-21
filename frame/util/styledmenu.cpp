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

#include "styledmenu.h"
#include "v23menustyle.h"
#include "constants.h"
#include "wayland/layershellhelper.h"
#include "wayland/layershell_styler.h"

#include <QAction>
#include <QDebug>
#include <QMouseEvent>
#include <QCursor>
#include <QApplication>
#include <QColor>
#include <QFontMetrics>
#include <QImage>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QStyleOptionMenuItem>
#include <QWindow>
#include <QVariantAnimation>

namespace {
constexpr int kContentInset = 12;
constexpr int kCornerRadius = 8;
// DStyle::PM_FrameRadius
constexpr int kV23CornerRadius = 8;
constexpr int kV23ShowDuration = 90;
constexpr qreal kV23ShowScale = 0.95;
constexpr int kShadowBlur = 5;
constexpr int kShadowOffsetY = 2;
}

QT_BEGIN_NAMESPACE
void qt_blurImage(QPainter *painter, QImage &image, qreal radius, bool quality,
                  bool alphaOnly, int transposed = 0);
QT_END_NAMESPACE

StyledMenu::StyledMenu(const QString &styleName, QWidget *parent)
    : QMenu(parent)
    , m_styleName(styleName)
{
    updateStyle();
    // V23DBG
    connect(this, &QMenu::hovered, this, [this](QAction *a) { qWarning() << "V23DBG hovered" << this << a->text(); });
    connect(this, &QMenu::aboutToHide, this, [this] { qWarning() << "V23DBG aboutToHide" << this; });

    if (Wayland::LayerShellHelper::isWayland()) {
        setAttribute(Qt::WA_TranslucentBackground);
        setContentsMargins(kContentInset, kContentInset,
                           kContentInset, kContentInset);
    }
}

void StyledMenu::updateStyle()
{
    QStyle *style = nullptr;
    if (Dock::isV23Mode()) {
        const bool light = Dock::isV23Light();
        V23MenuStyle *current = qobject_cast<V23MenuStyle *>(m_style);
        if (current && current->isLight() == light)
            return;
        style = new V23MenuStyle(light);
    } else {
        if (m_style && !qobject_cast<V23MenuStyle *>(m_style))
            return;
        style = QStyleFactory::create(m_styleName);
    }

    QStyle *old = m_style;
    if (style) {
        style->setParent(this);
        m_style = style;
    } else {
        m_style = QApplication::style();
    }
    setStyle(m_style);
    setProperty("_d_menu_radius", qobject_cast<V23MenuStyle *>(m_style) ? kV23CornerRadius : kCornerRadius);

    if (old && old != m_style && old->parent() == this)
        old->deleteLater();
}

void StyledMenu::updateStyles(QMenu *menu)
{
    if (!menu)
        return;

    if (StyledMenu *styled = dynamic_cast<StyledMenu *>(menu))
        styled->updateStyle();

    for (QMenu *sub : menu->findChildren<QMenu *>())
        if (StyledMenu *styled = dynamic_cast<StyledMenu *>(sub))
            styled->updateStyle();
}

bool StyledMenu::event(QEvent *e)  // V23DBG
{
    switch (e->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(e);
        qWarning() << "V23DBG move" << this << me->position() << me->globalPosition() << "active" << (activeAction() ? activeAction()->text() : QString());
        break; }
    case QEvent::Enter: case QEvent::Leave: case QEvent::Hide: case QEvent::Close:
        qWarning() << "V23DBG event" << this << e->type() << QCursor::pos();
        break;
    default: break;
    }
    return QMenu::event(e);
}

void StyledMenu::showEvent(QShowEvent *event)
{
    QMenu::showEvent(event);

    if (!Wayland::LayerShellHelper::isWayland() || !qobject_cast<V23MenuStyle *>(m_style))
        return;

    if (!m_showAnimation) {
        m_showAnimation = new QVariantAnimation(this);
        m_showAnimation->setStartValue(0.0);
        m_showAnimation->setEndValue(1.0);
        m_showAnimation->setDuration(kV23ShowDuration);
        m_showAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_showAnimation, &QVariantAnimation::valueChanged, this, [this] { update(); });
        connect(m_showAnimation, &QVariantAnimation::finished, this, &StyledMenu::applyV23Blur);
    }
    m_showAnimation->stop();
    m_showAnimation->start();
}

void StyledMenu::applyV23Blur()
{
    QWindow *window = windowHandle();
    if (!window || !isVisible())
        return;

    const QMargins margins = contentsMargins();
    Wayland::LayerShellStyler::apply(window, kV23CornerRadius, true,
        rect().marginsRemoved(margins));
    update();
}

QPointF StyledMenu::showAnimationOrigin(const QRectF &menuRect) const
{
    if (qobject_cast<QMenu *>(parentWidget()))
        return menuRect.topLeft();

    switch (QCoreApplication::instance()->property(PROP_POSITION).value<Dock::Position>()) {
    case Dock::Top:     return QPointF(menuRect.center().x(), menuRect.top());
    case Dock::Left:    return QPointF(menuRect.left(), menuRect.center().y());
    case Dock::Right:   return QPointF(menuRect.right(), menuRect.center().y());
    default:            return QPointF(menuRect.center().x(), menuRect.bottom());
    }
}

void StyledMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!m_style || !Wayland::LayerShellHelper::isWayland()) {
        QMenu::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QMargins margins = contentsMargins();
    const QRectF menuRect = QRectF(rect()).adjusted(
        margins.left(), margins.top(), -margins.right(), -margins.bottom());
    const V23MenuStyle *v23Style = qobject_cast<const V23MenuStyle *>(m_style);
    const int radius = v23Style ? kV23CornerRadius : kCornerRadius;
    QPainterPath menuPath;
    menuPath.addRoundedRect(menuRect, radius, radius);

    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.restore();

    if (v23Style && m_showAnimation && m_showAnimation->state() == QAbstractAnimation::Running) {
        const qreal progress = m_showAnimation->currentValue().toReal();
        const qreal scale = kV23ShowScale + (1.0 - kV23ShowScale) * progress;
        const QPointF origin = showAnimationOrigin(menuRect);
        painter.setOpacity(progress);
        painter.translate(origin);
        painter.scale(scale, scale);
        painter.translate(-origin);
    }

    if (!margins.isNull()) {
        if (m_shadowCache.isNull() || m_shadowCacheSize != size() || m_shadowCacheRadius != radius) {
            QImage shadow(size(), QImage::Format_ARGB32_Premultiplied);
            shadow.fill(Qt::transparent);
            {
                QPainter shadowPainter(&shadow);
                shadowPainter.setRenderHint(QPainter::Antialiasing, true);
                shadowPainter.fillPath(menuPath.translated(0, kShadowOffsetY),
                                       QColor(0, 0, 0));
            }

            m_shadowCache = QImage(size(), QImage::Format_ARGB32_Premultiplied);
            m_shadowCache.fill(Qt::transparent);
            QPainter cachePainter(&m_shadowCache);
            qt_blurImage(&cachePainter, shadow, kShadowBlur * 2.0, true, true);
            m_shadowCacheSize = size();
            m_shadowCacheRadius = radius;
        }

        QPainterPath outside;
        outside.addRect(rect());
        outside = outside.subtracted(menuPath);

        painter.save();
        painter.setClipPath(outside);
        painter.setOpacity(painter.opacity() * 0.20);
        painter.drawImage(0, 0, m_shadowCache);
        painter.restore();
    }

    const QColor baseWindow = v23Style ? v23Style->windowColor()
                                       : palette().color(QPalette::Window);
    QColor translucentWindow = baseWindow;
    const bool hasBlur = windowHandle()
        && windowHandle()->property("_d_wayland_has_blur").toBool();
    if (v23Style) {
        const qreal alpha = v23Style->isLight() ? 0.45 : 0.6;
        translucentWindow.setAlphaF(hasBlur || Wayland::LayerShellHelper::isWayland() ? alpha : 1.0);
    } else {
        translucentWindow.setAlphaF(hasBlur ? 0.45 : 0.92);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(translucentWindow);
    painter.drawPath(menuPath);

    painter.save();
    painter.setClipPath(menuPath);

    const QFont font = this->font();
    const QFontMetrics metrics(font);
    for (QAction *action : actions()) {
        if (!action->isVisible()) {
            continue;
        }

        QStyleOptionMenuItem option;
        option.initFrom(this);
        option.palette = palette();
        option.font = font;
        option.fontMetrics = metrics;
        option.text = action->text();
        option.icon = action->icon();
        option.menuItemType = action->isSeparator()
            ? QStyleOptionMenuItem::Separator
            : action->menu()
                ? QStyleOptionMenuItem::SubMenu
                : QStyleOptionMenuItem::Normal;
        option.checkType = action->isCheckable()
            ? QStyleOptionMenuItem::NonExclusive
            : QStyleOptionMenuItem::NotCheckable;
        option.checked = action->isChecked();
        option.maxIconWidth = m_style->pixelMetric(
            QStyle::PM_SmallIconSize, &option, this);
        option.menuRect = menuRect.toAlignedRect();
        option.rect = actionGeometry(action);
        option.styleObject = this;
        option.state = QStyle::State_Enabled;
        if (action == activeAction()) {
            option.state |= QStyle::State_Selected;
        }
        if (!action->isEnabled()) {
            option.state &= ~QStyle::State_Enabled;
        }

        painter.save();
        painter.setClipRect(option.rect, Qt::IntersectClip);
        m_style->drawControl(QStyle::CE_MenuItem, &option, &painter, this);
        painter.restore();
    }

    painter.restore();

    painter.setBrush(Qt::NoBrush);
    if (v23Style) {
        const bool light = v23Style->isLight();
        painter.setPen(light ? QColor(0, 0, 0, 255 * 0.12) : QColor(0, 0, 0, 255 * 0.5));
        painter.drawPath(menuPath);
        painter.setPen(light ? QColor(255, 255, 255, 255 * 0.6) : QColor(255, 255, 255, 255 * 0.1));
        painter.drawRoundedRect(menuRect.adjusted(1, 1, -1, -1), kV23CornerRadius - 1, kV23CornerRadius - 1);
        return;
    }

    painter.setPen(QColor(0, 0, 0, 25));
    painter.drawPath(menuPath);

    const bool light = baseWindow.lightness() > 128;
    painter.setPen(light ? QColor(255, 255, 255, 46)
                         : QColor(255, 255, 255, 22));
    painter.drawRoundedRect(
        menuRect.adjusted(1.5, 1.5, -1.5, -1.5),
        kCornerRadius - 1, kCornerRadius - 1);
}
