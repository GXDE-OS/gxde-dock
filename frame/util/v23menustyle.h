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

#ifndef V23MENUSTYLE_H
#define V23MENUSTYLE_H

#include <QColor>
#include <QProxyStyle>
#include <QStyleOptionMenuItem>
#include <QVariantAnimation>

// Adapted under ChameleonMovementAnimation, origin from Deepin DDE qt6integration, licensed under GPLv3+
class V23MenuMovementAnimation : public QVariantAnimation
{
    Q_OBJECT

public:
    explicit V23MenuMovementAnimation(QWidget *targetWidget);

    QRect currentRect() const { return m_currentRect; }
    bool isRunning() const { return state() == QVariantAnimation::Running; }
    float progress() const { return float(currentLoopTime()) / duration(); }

    QWidget *targetWidget() const;
    void setTargetRect(const QRect &rect);

private:
    QRect m_currentRect;
    QRect m_targetRect;
    QRect m_lastTargetRect;
};

class V23MenuStyle : public QProxyStyle
{
    Q_OBJECT

public:
    explicit V23MenuStyle(const bool light);

    bool isLight() const { return m_light; }

    QColor windowColor() const;
    QColor highlightColor() const;
    QColor highlightedTextColor() const;
    QColor textColor() const;
    QColor secondaryTextColor() const;
    QColor separatorColor() const;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &contentsSize, const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget = nullptr) const override;

private:
    V23MenuMovementAnimation *drawMenuItemBackground(const QStyleOptionMenuItem *menuItem,
                                                     QPainter *painter) const;
    void drawMenuItem(const QStyleOptionMenuItem *menuItem, QPainter *painter,
                      const QWidget *widget) const;

    bool m_light;
};

#endif // V23MENUSTYLE_H
