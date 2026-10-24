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

#include "v23collapseitem.h"
#include "components/v23trayglyph.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

V23CollapseItem::V23CollapseItem(QWidget *parent)
        : DockItem(parent)
        , m_collapsed(false)
        , m_rotation(new QVariantAnimation(this))
        , m_tips(new TipsWidget(this)) {
    m_tips->setText(tr("Collapse tray"));
    m_tips->setVisible(false);
    m_tips->setObjectName("V23CollapseItem");
    m_rotation->setDuration(200);
    m_rotation->setStartValue(180.0);
    m_rotation->setEndValue(180.0);
    connect(m_rotation, &QVariantAnimation::valueChanged, this, [this] { update(); });

    setAccessibleName("CollapseTray");
}

void V23CollapseItem::setCollapsed(const bool collapsed) {
    if (collapsed == m_collapsed) {
        return;
    }

    m_collapsed = collapsed;

    m_rotation->stop();
    m_rotation->setStartValue(m_rotation->currentValue());
    m_rotation->setEndValue(collapsed ? 0.0 : 180.0);
    m_rotation->start();
}

void V23CollapseItem::paintEvent(QPaintEvent *e) {
    DockItem::paintEvent(e);

    QPainter painter(this);
    if (m_hover || m_pressed) {
        paintV23Plate(painter, v23TrayPlateRect(), 4, m_pressed);
    }

    QRectF icon(0, 0, 16, 16);
    icon.moveCenter(QRectF(rect()).center());
    const bool horizontal = DockPosition == Dock::Top || DockPosition == Dock::Bottom;
    V23TrayGlyph::paintExpand(painter, icon, horizontal, m_rotation->currentValue().toReal());
}

void V23CollapseItem::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !rect().contains(e->pos())) {
        return DockItem::mouseReleaseEvent(e);
    }

    hidePopup();
    emit clicked();
}

QSize V23CollapseItem::sizeHint() const {
    return QSize(24, 24);
}

QWidget *V23CollapseItem::popupTips() {
    return m_tips;
}
