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

#include <QAction>
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

namespace {
constexpr int kContentInset = 12;
constexpr int kCornerRadius = 8;
constexpr int kShadowBlur = 5;
constexpr int kShadowOffsetY = 2;
}

QT_BEGIN_NAMESPACE
void qt_blurImage(QPainter *painter, QImage &image, qreal radius, bool quality,
                  bool alphaOnly, int transposed = 0);
QT_END_NAMESPACE

StyledMenu::StyledMenu(const QString &styleName, QWidget *parent)
    : QMenu(parent)
{
    m_style = QStyleFactory::create(styleName);
    if (!m_style) {
        m_style = QApplication::style();
    } else {
        m_style->setParent(this);
    }

    if (m_style) {
        setStyle(m_style);
    }

    setAttribute(Qt::WA_TranslucentBackground);
    setContentsMargins(kContentInset, kContentInset,
                       kContentInset, kContentInset);
}

void StyledMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!m_style) {
        QMenu::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QMargins margins = contentsMargins();
    const QRectF menuRect = QRectF(rect()).adjusted(
        margins.left(), margins.top(), -margins.right(), -margins.bottom());
    QPainterPath menuPath;
    menuPath.addRoundedRect(menuRect, kCornerRadius, kCornerRadius);

    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.restore();

    if (!margins.isNull()) {
        QImage shadow(size(), QImage::Format_ARGB32_Premultiplied);
        shadow.fill(Qt::transparent);
        {
            QPainter shadowPainter(&shadow);
            shadowPainter.setRenderHint(QPainter::Antialiasing, true);
            shadowPainter.fillPath(menuPath.translated(0, kShadowOffsetY),
                                   QColor(0, 0, 0));
        }

        QPainterPath outside;
        outside.addRect(rect());
        outside = outside.subtracted(menuPath);

        painter.save();
        painter.setClipPath(outside);
        painter.setOpacity(0.20);
        qt_blurImage(&painter, shadow, kShadowBlur * 2.0, true, true);
        painter.restore();
    }

    const QColor baseWindow = palette().color(QPalette::Window);
    QColor translucentWindow = baseWindow;
    const bool hasBlur = windowHandle()
        && windowHandle()->property("_d_wayland_has_blur").toBool();
    translucentWindow.setAlphaF(hasBlur ? 0.45 : 0.92);
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

        m_style->drawControl(QStyle::CE_MenuItem, &option, &painter, this);
    }

    painter.restore();

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(0, 0, 0, 25));
    painter.drawPath(menuPath);

    const bool light = baseWindow.lightness() > 128;
    painter.setPen(light ? QColor(255, 255, 255, 46)
                         : QColor(255, 255, 255, 22));
    painter.drawRoundedRect(
        menuRect.adjusted(1.5, 1.5, -1.5, -1.5),
        kCornerRadius - 1, kCornerRadius - 1);
}
