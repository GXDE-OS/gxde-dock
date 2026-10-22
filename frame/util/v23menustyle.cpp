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

#include "v23menustyle.h"

#include <QCommonStyle>
#include <QFontInfo>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>

// Adapted under ChameleonMovementAnimation, origin from Deepin DDE qt6integration, licensed under GPLv3+
namespace {
// metrics of DStyle and the chameleon menu (qt6integration common.h)
constexpr int FrameRadius = 8;              // DStyle::PM_FrameRadius
constexpr int SmallIconSize = 14;           // DStyle PM_SmallIconSize
constexpr int ButtonIconSize = 16;          // DStyle PM_ButtonIconSize
constexpr int MenuVMargin = 8;              // PM_MenuVMargin
constexpr int MenuArrowHMargin = 6;         // Menu_ArrowHMargin
constexpr int MenuItemHTextMargin = 3;      // Menu_ItemHTextMargin
constexpr int MenuItemVTextMargin = 4;      // Menu_ItemVTextMargin
constexpr int MenuPanelRightPadding = 10;   // Menu_PanelRightPadding
constexpr int MenuItemTabSpacing = 16;      // MenuItem_TabSpacing
constexpr int MenuItemMinWidth = 162;
constexpr int ItemBackgroundMargin = 6;
constexpr int ItemBackgroundRadius = 6;
constexpr int SeparatorHeight = 2;

// DStyle::adjustColor(color, 0, 0, 0, 0, 0, 0, -60) for disabled items
QColor disabledColor(QColor color)
{
    color.setAlphaF(color.alphaF() * 0.4);
    return color;
}

// mark_indicator_12px.svg of the DTK builtin icons: a 14x12 check mark
void drawMark(QPainter *painter, const QRectF &rect, const QColor &color)
{
    const qreal sx = rect.width() / 14.0;
    const qreal sy = rect.height() / 12.0;
    const QPointF points[] = {
        rect.topLeft() + QPointF(0.935 * sx, 6.807 * sy),
        rect.topLeft() + QPointF(5.067 * sx, 10.839 * sy),
        rect.topLeft() + QPointF(13.435 * sx, 1.0 * sy),
    };

    QPen pen(color, 2 * qMin(sx, sy), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(points, 3);
    painter->restore();
}

// go-next_12px.svg of the DTK builtin icons, fitted into rect like QIcon::paint
void drawArrowEnter(QPainter *painter, const QRect &rect, const QColor &color)
{
    const int size = qMin(rect.width(), rect.height());
    const int dx = (rect.width() - size) / 2;
    const int dy = (rect.height() - size) / 2;
    const QRectF iconRect(rect.x() + dx, rect.y() + dy, size, size);
    const qreal scale = size / 12.0;
    const QPointF points[] = {
        iconRect.topLeft() + QPointF(4 * scale, 1 * scale),
        iconRect.topLeft() + QPointF(8.826 * scale, 5.95 * scale),
        iconRect.topLeft() + QPointF(4 * scale, 10.899 * scale),
    };

    QPen pen(color, 1.2 * scale, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(points, 3);
    painter->restore();
}
}

V23MenuMovementAnimation::V23MenuMovementAnimation(QWidget *targetWidget)
    : QVariantAnimation(targetWidget)
{
    // chameleon: 150ms linear; shorter and front-loaded so the highlight
    // keeps up with the pointer
    setDuration(100);
    setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QVariantAnimation::valueChanged, targetWidget, [this] (const QVariant &value) {
        if (!isRunning())
            return;

        const QRect rect = value.toRect();
        this->targetWidget()->update(m_currentRect.united(rect));
        m_currentRect = rect;
    });
    connect(this, &QVariantAnimation::finished, targetWidget, [this] {
        // the selected item's text switches to the highlighted color only
        // once the animation is over, make sure that frame is painted
        this->targetWidget()->update(m_currentRect);
    });
}

QWidget *V23MenuMovementAnimation::targetWidget() const
{
    return qobject_cast<QWidget *>(parent());
}

void V23MenuMovementAnimation::setTargetRect(const QRect &rect)
{
    if (m_targetRect == rect)
        return;

    m_lastTargetRect = m_targetRect;
    m_targetRect = rect;

    if (m_currentRect.isEmpty())
        m_currentRect = m_lastTargetRect;

    // a new target makes the running animation obsolete, restart from here
    stop();
    setStartValue(m_currentRect);
    setEndValue(rect);

    if (!m_currentRect.isEmpty()) {
        start();
    } else {
        // first selection after the menu is shown, no movement
        m_currentRect = rect;
    }
}

V23MenuStyle::V23MenuStyle(const bool light)
    : QProxyStyle(QStyleFactory::create(light ? QStringLiteral("dlight2") : QStringLiteral("ddark2")))
    , m_light(light)
{
}

// colors of the DTK standard palettes (DGuiApplicationHelper::standardPalette)
QColor V23MenuStyle::windowColor() const
{
    return m_light ? QColor(0xf8, 0xf8, 0xf8) : QColor(0x25, 0x25, 0x25);
}

QColor V23MenuStyle::highlightColor() const
{
    return m_light ? QColor(0x00, 0x81, 0xff) : QColor(0x02, 0x4c, 0xca);
}

QColor V23MenuStyle::highlightedTextColor() const
{
    return m_light ? QColor(0xff, 0xff, 0xff) : QColor(0xf1, 0xf6, 0xff);
}

QColor V23MenuStyle::textColor() const
{
    // QPalette::BrightText
    return m_light ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor V23MenuStyle::secondaryTextColor() const
{
    // QPalette::WindowText
    return m_light ? QColor(0, 0, 0, 0xb2) : QColor(255, 255, 255, 0xb2);
}

QColor V23MenuStyle::separatorColor() const
{
    return m_light ? QColor(0, 0, 0, 255 * 0.1) : QColor(255, 255, 255, 255 * 0.05);
}

int V23MenuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_MenuVMargin:
        return MenuVMargin;
    case PM_MenuHMargin:
    case PM_MenuPanelWidth:
    case PM_SubMenuOverlap:
        return 0;
    case PM_SmallIconSize:
        return SmallIconSize;
    case PM_ButtonIconSize:
        return ButtonIconSize;
    default:
        break;
    }

    return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize V23MenuStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                     const QSize &contentsSize, const QWidget *widget) const
{
    const QStyleOptionMenuItem *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (type != CT_MenuItem || !menuItem)
        return QProxyStyle::sizeFromContents(type, option, contentsSize, widget);

    // chameleon starts from QCommonStyle, which gives the item height
    static QCommonStyle commonStyle;
    QSize size = commonStyle.sizeFromContents(type, option, contentsSize, widget);

    const int realMargins = SmallIconSize + 2 * FrameRadius;
    int width = realMargins;

    if (menuItem->text.contains(QLatin1Char('\t')))
        width += MenuItemTabSpacing;
    else if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
        width += 2 * MenuArrowHMargin;

    if (!menuItem->text.isEmpty())
        width += option->fontMetrics.size(Qt::TextSingleLine, menuItem->text).width() + FrameRadius;

    if (!menuItem->icon.isNull())
        width += SmallIconSize + FrameRadius;

    width += SmallIconSize + FrameRadius;
    size.setWidth(qMax(MenuItemMinWidth, width));

    if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
        size.setHeight(menuItem->text.isEmpty() ? SeparatorHeight : menuItem->fontMetrics.height());

    return size;
}

void V23MenuStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                 QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case PE_PanelMenu:
        painter->fillRect(option->rect, windowColor());
        return;
    case PE_FrameMenu:
        return;
    default:
        break;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void V23MenuStyle::drawControl(ControlElement element, const QStyleOption *option,
                               QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case CE_MenuItem:
        if (const QStyleOptionMenuItem *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            drawMenuItem(menuItem, painter, widget);
            return;
        }
        break;
    case CE_MenuEmptyArea:
        return;
    default:
        break;
    }

    QProxyStyle::drawControl(element, option, painter, widget);
}

// port of ChameleonStyle::drawMenuItemBackground()
V23MenuMovementAnimation *V23MenuStyle::drawMenuItemBackground(const QStyleOptionMenuItem *menuItem,
                                                               QPainter *painter) const
{
    const QRect rect = menuItem->rect.marginsRemoved(QMargins(ItemBackgroundMargin, 0, ItemBackgroundMargin, 0));
    const bool selected = (menuItem->state & State_Enabled) && (menuItem->state & State_Selected);

    if (!selected && menuItem->menuItemType == QStyleOptionMenuItem::Separator)
        painter->fillRect(rect, separatorColor());

    // every item paints its part of the moving highlight
    QWidget *target = qobject_cast<QWidget *>(menuItem->styleObject);
    if (!target)
        target = dynamic_cast<QWidget *>(painter->device());

    V23MenuMovementAnimation *animation = nullptr;
    if (target) {
        animation = target->findChild<V23MenuMovementAnimation *>(QStringLiteral("_d_menu_select_animation"),
                                                                 Qt::FindDirectChildrenOnly);
        if (!animation) {
            animation = new V23MenuMovementAnimation(target);
            animation->setObjectName(QStringLiteral("_d_menu_select_animation"));
        }

        if (selected)
            animation->setTargetRect(rect);
    }

    if (animation && animation->isRunning()) {
        painter->save();
        painter->setOpacity(1.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(highlightColor());
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(animation->currentRect(), ItemBackgroundRadius, ItemBackgroundRadius);
        painter->restore();
        return animation;
    }

    if (selected) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(highlightColor());
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(rect, ItemBackgroundRadius, ItemBackgroundRadius);
        painter->restore();
    }

    return nullptr;
}

// port of ChameleonStyle::drawMenuItem()
void V23MenuStyle::drawMenuItem(const QStyleOptionMenuItem *menuItem, QPainter *painter,
                                const QWidget *widget) const
{
    const QRect menuRect = menuItem->rect;
    const bool enabled = menuItem->state & State_Enabled;
    const bool selected = (menuItem->state & State_Selected) && enabled;
    const bool checkable = menuItem->checkType != QStyleOptionMenuItem::NotCheckable;
    const bool checked = menuItem->checked;
    const bool sunken = menuItem->state & State_Sunken;

    painter->save();

    V23MenuMovementAnimation *animation = drawMenuItemBackground(menuItem, painter);

    if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
        if (!menuItem->text.isEmpty()) {
            painter->setFont(menuItem->font);
            painter->setPen(enabled ? secondaryTextColor() : disabledColor(secondaryTextColor()));
            painter->drawText(menuRect, Qt::AlignLeft | Qt::AlignVCenter, menuItem->text);
        }

        painter->restore();
        return;
    }

    // while the highlight moves, the selected item fades its normal colored
    // content out and gets the highlighted text color when it arrives
    const bool useHighlightedText = selected && !animation;
    if (selected && animation)
        painter->setOpacity(1.0 - animation->progress());

    const QColor foreground = useHighlightedText ? highlightedTextColor()
                                                 : enabled ? textColor() : disabledColor(textColor());

    // the check mark column is always reserved
    const int realMargins = ButtonIconSize + 2 * FrameRadius;

    if (checkable && (checked || ((menuItem->checkType & QStyleOptionMenuItem::Exclusive) && sunken))) {
        // chameleon uses an absolute x here as its items start at 0; the
        // dock menus inset the items by their shadow margins
        QRect checkRect(menuItem->rect);
        checkRect.setLeft(menuRect.left() + FrameRadius);
        checkRect.setWidth(ButtonIconSize - 2);
        checkRect.setHeight(ButtonIconSize - 4);
        checkRect.moveCenter(QPoint(checkRect.left() + ButtonIconSize / 2, menuItem->rect.center().y()));
        drawMark(painter, checkRect, useHighlightedText ? highlightedTextColor() : textColor());
    }

    // icon
    QSize iconSize(0, 0);
    if (!menuItem->icon.isNull()) {
        iconSize = QSize(ButtonIconSize, ButtonIconSize);
        const QRect iconRect(menuRect.x() + realMargins, menuRect.center().y() - iconSize.height() / 2,
                             iconSize.width(), iconSize.height());
        menuItem->icon.paint(painter, iconRect, Qt::AlignCenter,
                             enabled ? QIcon::Normal : QIcon::Disabled, checked ? QIcon::On : QIcon::Off);
    }

    // text
    int x, y, w, h;
    menuRect.getRect(&x, &y, &w, &h);
    const int tab = menuItem->reservedShortcutWidth;
    int xpos = menuRect.x();
    xpos += iconSize.width() > 0 ? realMargins + FrameRadius + iconSize.width() : realMargins;

    const QRect textRect(xpos, y + MenuItemHTextMargin, w - xpos - tab, h - 2 * MenuItemVTextMargin);
    QString text = menuItem->text;
    if (!text.isEmpty()) {
        int textFlags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignLeft;
        if (!proxy()->styleHint(SH_UnderlineShortcut, menuItem, widget))
            textFlags |= Qt::TextHideMnemonic;

        painter->setPen(foreground);
        painter->setBrush(Qt::NoBrush);

        const int tabIndex = text.indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            QPoint shortcutStart = textRect.topRight();
            shortcutStart.setX(shortcutStart.x() - MenuPanelRightPadding - realMargins);
            const QRect shortcutRect(shortcutStart, QPoint(menuRect.right(), textRect.bottom()));
            painter->setFont(menuItem->font);
            painter->drawText(shortcutRect, textFlags, text.mid(tabIndex + 1));
            text = text.left(tabIndex);
        }

        QFont font = menuItem->font;
        font.setPointSizeF(QFontInfo(menuItem->font).pointSizeF());
        painter->setFont(font);
        painter->drawText(textRect, textFlags, text);
    }

    // sub menu arrow
    if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu) {
        const QPoint topLeft(menuItem->rect.right() - FrameRadius - ButtonIconSize / 2,
                             menuItem->rect.center().y() - ButtonIconSize / 3);
        const QPoint bottomRight(topLeft.x() + ButtonIconSize / 2,
                                 menuItem->rect.center().y() + ButtonIconSize / 3);
        // DTK builtin "texts" icons take the color of the current pen
        drawArrowEnter(painter, QRect(topLeft, bottomRight), foreground);
    }

    painter->restore();
}
