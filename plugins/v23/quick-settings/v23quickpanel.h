// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23QUICKPANEL_H
#define V23QUICKPANEL_H

#include <QHash>
#include <QWidget>

class QParallelAnimationGroup;
class QVariantAnimation;
class V23CircleButton;
class V23DetailPage;
class V23QuickModule;

// The popup of the quick settings: the tiles and rows of the modules and the
// settings and power buttons, or a page of a module in place of them
class V23QuickPanel : public QWidget
{
    Q_OBJECT

public:
    explicit V23QuickPanel(const QList<V23QuickModule *> &modules, QWidget *parent = nullptr);

    void showPage(V23QuickModule *module);
    void showMain();

signals:
    void requestHide() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *e) override;

private:
    void relayout();
    void setCurrent(QWidget *page);
    void slideTo(QWidget *page, bool forward);
    void setPanelHeight(int height);

private:
    QList<V23QuickModule *> m_modules;
    QWidget *m_main;
    QWidget *m_current;
    QList<QPair<V23QuickModule *, QWidget *>> m_items;  // in the order they show
    V23CircleButton *m_settingsButton;
    V23CircleButton *m_powerButton;
    QHash<V23QuickModule *, V23DetailPage *> m_pages;
    QParallelAnimationGroup *m_animation;
    QVariantAnimation *m_heightAnimation;
};

#endif // V23QUICKPANEL_H
