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

#ifndef DOCKPOPUPWINDOW_H
#define DOCKPOPUPWINDOW_H

#include <darrowrectangle.h>
#include <dregionmonitor.h>
#include <DWindowManagerHelper>
#include <QWidget>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QPointer>

class DockPopupMask;
class DockPopupAnimationOverlay;
class QPainter;
class QVariantAnimation;

DWIDGET_USE_NAMESPACE
DTK_USE_NAMESPACE

class DockPopupWindow : public Dtk::Widget::DArrowRectangle
{
    Q_OBJECT

public:
    explicit DockPopupWindow(QWidget *parent = 0);
    ~DockPopupWindow();

    bool model() const;

    void setContent(QWidget *content);

public slots:
    void show(const QPoint &pos, const bool model = false);
    void show(const int x, const int y);
    void hide();

signals:
    void accept() const;
    // 在把专业版的仓库降级到debian的stable时, dock出现了一个奇怪的问题:
    // 在plugins/tray/system-trays/systemtrayitem.cpp中的showPopupWindow函数中
    // 无法连接到上面这个信号: "accept", qt给出一个运行时警告提示找不到信号
    // 目前的解决方案就是在下面增加了这个信号
    void unusedSignal();

protected:
    void showEvent(QShowEvent *e);
    void enterEvent(QEnterEvent *e);
    bool eventFilter(QObject *o, QEvent *e);
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onGlobMouseRelease(const QPoint &mousePos, const int flag);
    void compositeChanged();
    void ensureRaised();

private:
    friend class DockPopupAnimationOverlay;

    void updateV23Style(QWidget *content);
    QRect v23PanelRect() const;
    void applyV23Blur();
    void hideNow();

    // V23 mode on Wayland: the fade of the popups that deepin-kwin draws on
    // Deepin V23, drawn by ourselves as the GXDE compositor has none
    enum class Animation {
        None,
        Opening,
        Closing,
    };
    void startAnimation(Animation animation);
    void stopAnimation();
    QRectF animatedPanelRect(qreal progress) const;
    void paintAnimation(QPainter &painter);

private:
    bool m_model;
    // V23 mode: Deepin 25 panel without arrow, painted by ourselves
    bool m_v23;
    int m_v23ShadowOffset;
    QImage m_shadowCache;
    QSize m_shadowCacheSize;
    QPoint m_lastPoint;
    // where the popup points at, in its own coordinates
    QPoint m_anchor;

    Animation m_animation;
    QVariantAnimation *m_animator;
    DockPopupAnimationOverlay *m_animationOverlay;
    QPixmap m_snapshot;
    QPointer<QWidget> m_openedContent;

    QTimer *m_acceptDelayTimer;

    DRegionMonitor *m_regionInter;
    DWindowManagerHelper *m_wmHelper;

    DockPopupMask *m_maskWindow;
};

class DockPopupMask : public QWidget 
{
    Q_OBJECT
public:
    explicit DockPopupMask(DockPopupWindow *popup, QWidget *parent = nullptr);
    void showMask(QScreen *screen);
    void hideMask();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    DockPopupWindow *m_popup;
};

#endif // DOCKPOPUPWINDOW_H
