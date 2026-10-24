/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             listenerri <listenerri@gmail.com>
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

#ifndef MAINPANEL_H
#define MAINPANEL_H

#include "constants.h"
#include "controller/dockitemcontroller.h"
#include "util/docksettings.h"
#include "item/showdesktopitem.h"

#include <QFrame>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QBoxLayout>
#include <QPainter>

#include <functional>

#include <DBlurEffectWidget>
#include <DWindowManagerHelper>

#define xstr(s) str(s)
#define str(s) #s
#define PANEL_BORDER    0
#define PANEL_PADDING   0
#define PANEL_MARGIN    1

DWIDGET_USE_NAMESPACE

class MainPanel : public DBlurEffectWidget
{
    Q_OBJECT
    Q_PROPERTY(int displayMode READ displayMode DESIGNABLE true)
    Q_PROPERTY(int position READ position DESIGNABLE true)

public:
    // 多屏: controller 为当前屏幕的 DockItemController
    explicit MainPanel(DockItemController *controller, QWidget *parent = 0);
    virtual ~MainPanel();

    void updateDockPosition(const Position dockPosition);
    void updateDockDisplayMode(const Dock::DisplayMode displayMode);
    int displayMode() const;
    int position() const;

    void setEffectEnabled(const bool enabled);

    bool eventFilter(QObject *watched, QEvent *event) override;

    void setFixedSize(const QSize &size);
    void setComposite(const bool hasComposite);

    inline const QList<QRect> &v20Capsules() const {
        return m_v20Capsules;
    }

signals:
    void requestWindowAutoHide(const bool autoHide) const;
    void requestRefershWindowVisible() const;
    void geometryChanged();
    void v20CapsulesChanged() const;

private:
    void moveEvent(QMoveEvent *e);
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *e) override;
    void updateShowDesktopItemSize();
    void showEvent(QShowEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e);
    void dragMoveEvent(QDragMoveEvent *e);
    void dragLeaveEvent(QDragLeaveEvent *e);
    void dropEvent(QDropEvent *e);

    void manageItem(DockItem *item);
    DockItem *itemAt(const QPoint &point);
    void animateItemMoves(const std::function<void()> &change);
    void updateV23TrayVisibility(const bool animate);
    QMargins v20Margins() const;
    int v20CapsulePadding() const;
    void updateV20Capsules();

private slots:
    void adjustItemSize();
    void updateV23Alignment();
    int layoutIndex(const int index) const;
    void itemInserted(const int index, DockItem *item);
    void itemRemoved(DockItem *item);
    void itemMoved(DockItem *item, const int index);
    void itemDragStarted();
    void itemDropped(QObject *destnation, const QPoint &dropPoint);
    void handleDragMove(QDragMoveEvent *e, bool isFilter);
    void checkMouseReallyLeave();

private:
    QSize resizeDockPlugins(QSize oldSize, Dock::Position dockPlace, QSize suggestSize);

    Position m_position;
    DisplayMode m_displayMode;
    bool m_hasComposite = false;

    QBoxLayout *m_itemLayout;
    QSpacerItem *m_v23CenterSpacer = nullptr;
    QPointer<DockItem> m_v23SearchItem;
    QTimer *m_itemAdjustTimer;
    QTimer *m_checkMouseLeaveTimer;
    QPointer<QWidget> m_appDragWidget;
    QVariantAnimation *m_sizeChangeAni;

    ShowDesktopItem *m_showDesktopItem;
    DockItemController *m_itemController;

    QSet<QWidget *> m_v23ShowingItems;
    QList<QRect> m_v20Capsules;
    QTimer *m_v20CapsuleTimer;

    QString m_draggingMimeKey;
    QSize m_destSize;
};

#endif // MAINPANEL_H
