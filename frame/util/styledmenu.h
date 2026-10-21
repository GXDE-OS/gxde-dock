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

#ifndef STYLEDMENU_H
#define STYLEDMENU_H

#include <QImage>
#include <QMenu>

class QStyle;
class QVariantAnimation;

class StyledMenu : public QMenu
{
public:
    explicit StyledMenu(const QString &styleName, QWidget *parent = nullptr);
    void updateStyle();
    static void updateStyles(QMenu *menu);

protected:
    bool event(QEvent *e) override; // V23DBG
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void applyV23Blur();
    QPointF showAnimationOrigin(const QRectF &menuRect) const;

    QString m_styleName;
    QStyle *m_style = nullptr;
    QVariantAnimation *m_showAnimation = nullptr;
    QImage m_shadowCache;
    QSize m_shadowCacheSize;
    int m_shadowCacheRadius = -1;
};

#endif // STYLEDMENU_H
