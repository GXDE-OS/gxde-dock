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

#ifndef V23COLLAPSEITEM_H
#define V23COLLAPSEITEM_H

#include "dockitem.h"
#include "../widgets/tipswidget.h"

class QVariantAnimation;
class V23CollapseItem : public DockItem {
    Q_OBJECT

public:
    explicit V23CollapseItem(QWidget *parent = nullptr);

    inline ItemType itemType() const override {
        return TrayAction;
    }

    void setCollapsed(const bool collapsed);

signals:
    void clicked() const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    QSize sizeHint() const override;
    QWidget *popupTips() override;

private:
    bool m_collapsed;
    QVariantAnimation *m_rotation;
    TipsWidget *m_tips;
};

#endif // V23COLLAPSEITEM_H
