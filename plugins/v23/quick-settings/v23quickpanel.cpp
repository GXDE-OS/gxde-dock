// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Lays the modules out like PanelPluginPage.qml of dde-shell: a flow 330
// wide with 10px padding and spacing, the two column tiles first, then the
// single ones, then the full width rows (QuickPanelProxyModel), and a row
// with the settings and power buttons at the bottom.

#include "v23quickpanel.h"
#include "v23quickmodule.h"

#include <DDBusSender>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <algorithm>

using namespace V23Quick;

namespace {
constexpr int BarButtonSize = 30;
// the StackView of dtkdeclarative in Deepin V23
constexpr int SlideDuration = 200;
// the popups fade in this long with deepin-kwin, the panel grows alike
constexpr int ResizeDuration = 150;
}

V23QuickPanel::V23QuickPanel(const QList<V23QuickModule *> &modules, QWidget *parent)
    : QWidget(parent)
    , m_modules(modules)
    , m_main(new QWidget(this))
    , m_current(nullptr)
    , m_settingsButton(new V23CircleButton(V23CircleButton::Flat, BarButtonSize, QSize(16, 16), m_main))
    , m_powerButton(new V23CircleButton(V23CircleButton::Flat, BarButtonSize, QSize(16, 16), m_main))
    , m_animation(new QParallelAnimationGroup(this))
    , m_heightAnimation(new QVariantAnimation(this))
{
    m_heightAnimation->setDuration(ResizeDuration);
    m_heightAnimation->setEasingCurve(QEasingCurve::OutQuart);
    connect(m_heightAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setFixedHeight(value.toInt());
    });

    setFixedWidth(PanelWidth);
    m_main->setFixedWidth(PanelWidth);

    // large tiles, single tiles, then full width rows; the modules keep their
    // order inside each kind, the rows follow rowOrder()
    QList<QPair<V23QuickModule *, QWidget *>> large, single, rows;
    for (V23QuickModule *module : m_modules) {
        if (QWidget *tile = module->createTile(m_main)) {
            auto *quickTile = qobject_cast<V23QuickTile *>(tile);
            (quickTile && quickTile->style() == V23QuickTile::Large ? large : single) << qMakePair(module, tile);
        }
        if (QWidget *row = module->createRow(m_main))
            rows << qMakePair(module, row);

        connect(module, &V23QuickModule::availableChanged, this, &V23QuickPanel::relayout);
        connect(module, &V23QuickModule::requestPage, this, [this, module] {
            showPage(module);
        });
    }
    std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        return a.first->rowOrder() < b.first->rowOrder();
    });
    m_items << large << single << rows;

    m_settingsButton->setIcon(QStringLiteral("quickpanel-setting"));
    m_powerButton->setIcon(QStringLiteral("quickpanel-power"));
    connect(m_settingsButton, &V23CircleButton::clicked, this, [this] {
        emit requestHide();
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("Show"))
            .call();
    });
    connect(m_powerButton, &V23CircleButton::clicked, this, [this] {
        emit requestHide();
        DDBusSender()
            .service("com.deepin.dde.shutdownFront")
            .interface("com.deepin.dde.shutdownFront")
            .path("/com/deepin/dde/shutdownFront")
            .method(QString("Show"))
            .call();
    });

    relayout();
    setCurrent(m_main);
}

void V23QuickPanel::relayout()
{
    // the Flow of PanelPluginPage
    int x = PanelSpacing;
    int y = PanelSpacing;
    int rowHeight = 0;
    for (const auto &item : m_items) {
        QWidget *widget = item.second;
        const bool available = item.first->isAvailable();
        widget->setVisible(available);
        if (!available)
            continue;

        if (x + widget->width() > PanelWidth - PanelSpacing) {
            x = PanelSpacing;
            y += rowHeight + PanelSpacing;
            rowHeight = 0;
        }
        widget->move(x, y);
        x += widget->width() + PanelSpacing;
        rowHeight = qMax(rowHeight, widget->height());
    }
    if (rowHeight > 0)
        y += rowHeight + PanelSpacing;

    m_settingsButton->move(PanelSpacing, y);
    m_powerButton->move(PanelWidth - PanelSpacing - BarButtonSize, y);
    m_main->setFixedHeight(y + BarButtonSize + PanelSpacing);

    // a page of a module that is gone
    for (auto it = m_pages.constBegin(); it != m_pages.constEnd(); ++it) {
        if (m_current == it.value() && !it.key()->isAvailable())
            showMain();
    }

    if (m_current == m_main)
        setPanelHeight(m_main->height());
}

void V23QuickPanel::showPage(V23QuickModule *module)
{
    V23DetailPage *page = m_pages.value(module);
    if (!page) {
        QWidget *content = module->createPage(nullptr);
        if (!content)
            return;

        page = new V23DetailPage(module->displayName(), content, true, this);
        page->installEventFilter(this);
        connect(page, &V23DetailPage::back, this, &V23QuickPanel::showMain);
        m_pages.insert(module, page);
    }

    // SubPluginPage keeps at least the height of the panel, and 360
    page->setMinimumPageHeight(qMax(360, m_main->height()));
    slideTo(page, true);
}

void V23QuickPanel::showMain()
{
    if (isVisible())
        slideTo(m_main, false);
    else
        setCurrent(m_main);
}

// StackView.PushTransition of dtkdeclarative: the new page comes from half a
// width aside and fades in, the old one goes half a width the other way and
// fades out; going back mirrors it
void V23QuickPanel::slideTo(QWidget *page, bool forward)
{
    QWidget *previous = m_current;
    setCurrent(page);
    m_animation->clear();
    if (!previous || previous == page)
        return;

    previous->setVisible(true);
    const int offset = width() / 2;
    auto slide = [this](QWidget *widget, int from, int to, qreal fromOpacity, qreal toOpacity) {
        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(widget);
        effect->setOpacity(fromOpacity);
        widget->setGraphicsEffect(effect);

        QPropertyAnimation *move = new QPropertyAnimation(widget, "pos");
        move->setStartValue(QPoint(from, 0));
        move->setEndValue(QPoint(to, 0));
        QPropertyAnimation *fade = new QPropertyAnimation(effect, "opacity");
        fade->setStartValue(fromOpacity);
        fade->setEndValue(toOpacity);
        for (QPropertyAnimation *animation : {move, fade}) {
            animation->setDuration(SlideDuration);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            m_animation->addAnimation(animation);
        }
    };
    slide(page, forward ? offset : -offset, 0, 0.0, 1.0);
    slide(previous, 0, forward ? -offset : offset, 1.0, 0.0);
    page->move(forward ? offset : -offset, 0);

    connect(m_animation, &QParallelAnimationGroup::finished, this, [this, page, previous] {
        previous->setVisible(previous == m_current);
        previous->move(0, 0);
        previous->setGraphicsEffect(nullptr);
        page->setGraphicsEffect(nullptr);
    }, Qt::SingleShotConnection);
    m_animation->start();
}

void V23QuickPanel::setCurrent(QWidget *page)
{
    // a slide cut short leaves its pages where and how faded they were
    if (m_animation->state() == QAbstractAnimation::Running) {
        disconnect(m_animation, &QParallelAnimationGroup::finished, this, nullptr);
        m_animation->stop();
        m_animation->clear();

        QList<QWidget *> pages { m_main };
        for (V23DetailPage *detail : m_pages)
            pages << detail;
        for (QWidget *widget : pages) {
            widget->setGraphicsEffect(nullptr);
            widget->move(0, 0);
        }
    }

    m_current = page;
    m_main->setVisible(page == m_main);
    for (V23DetailPage *detail : m_pages)
        detail->setVisible(detail == page);

    page->move(0, 0);
    setPanelHeight(page->height());
}

// the popup follows the panel, so it grows and shrinks smoothly too
void V23QuickPanel::setPanelHeight(int height)
{
    m_heightAnimation->stop();
    if (!isVisible() || height == this->height()) {
        setFixedHeight(height);
        return;
    }

    m_heightAnimation->setStartValue(this->height());
    m_heightAnimation->setEndValue(height);
    m_heightAnimation->start();
}

bool V23QuickPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_current && event->type() == QEvent::Resize)
        setPanelHeight(m_current->height());

    return QWidget::eventFilter(watched, event);
}

void V23QuickPanel::showEvent(QShowEvent *e)
{
    // the panel always opens on its tiles
    if (m_current != m_main)
        showMain();

    QWidget::showEvent(e);
}
