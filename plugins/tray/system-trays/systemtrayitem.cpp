/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "systemtrayitem.h"
#include "dbus/dbusmenu.h"
#include "util/menudismissmask.h"
#include "util/styledmenu.h"
#include "wayland/layershellhelper.h"

#include <QGraphicsEffect>
#include <QPainter>
#include <QProcess>
#include <QDebug>
#include <QGuiApplication>
#include <QScopedPointer>
#include <QScreen>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <xcb/xproto.h>

namespace {

constexpr qreal DiskMountIconScale = 0.56;

class ScaledContentsEffect final : public QGraphicsEffect
{
public:
    explicit ScaledContentsEffect(qreal scale, QObject *parent = nullptr)
        : QGraphicsEffect(parent)
        , m_scale(scale)
    {
    }

protected:
    void draw(QPainter *painter) override
    {
        QPoint offset;
        const QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset, NoPad);
        if (pixmap.isNull())
            return;

        const QSizeF sourceSize = pixmap.deviceIndependentSize();
        const QRectF sourceBounds(QPointF(offset), sourceSize);
        QRectF targetBounds(QPointF(), sourceSize * m_scale);
        targetBounds.moveCenter(sourceBounds.center());

        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(targetBounds, pixmap, QRectF(pixmap.rect()));
        painter->restore();
    }

private:
    qreal m_scale;
};

}

Dock::Position SystemTrayItem::DockPosition = Dock::Position::Top;
QPointer<DockPopupWindow> SystemTrayItem::PopupWindow = nullptr;

SystemTrayItem::SystemTrayItem(PluginsItemInterface * const pluginInter, const QString &itemKey, QWidget *parent)
    : AbstractTrayWidget(parent),
      m_popupShown(false),
      m_tapAndHold(false),
      m_pluginInter(pluginInter),
      m_menuManagerInter(new DBusMenuManager(this)),
      m_centralWidget(m_pluginInter->itemWidget(itemKey)),
      m_popupTipsDelayTimer(new QTimer(this)),
      m_popupAdjustDelayTimer(new QTimer(this)),
      m_itemKey(itemKey)
{
    qDebug() << "load tray plugins item: " << m_pluginInter->pluginName() << itemKey << m_centralWidget;

    m_centralWidget->setParent(this);
    m_centralWidget->setVisible(true);
    m_centralWidget->installEventFilter(this);

    // The external disk-mount plugin paints a noticeably larger symbol than
    // the other system tray plugins. Scale only its visual contents; keep the
    // widget geometry unchanged so its hit area and popup anchoring stay put.
    if (m_pluginInter->pluginName() == "disk-mount")
        m_centralWidget->setGraphicsEffect(
            new ScaledContentsEffect(DiskMountIconScale, m_centralWidget));

    QBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addWidget(m_centralWidget);
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(hLayout);
    setAccessibleName(m_pluginInter->pluginName() + "-" + m_itemKey);
    setAttribute(Qt::WA_TranslucentBackground);

    if (PopupWindow.isNull())
    {
        DockPopupWindow *arrowRectangle = new DockPopupWindow(nullptr);
        arrowRectangle->setShadowBlurRadius(20);
        arrowRectangle->setRadius(6);
        arrowRectangle->setShadowYOffset(2);
        arrowRectangle->setShadowXOffset(0);
        arrowRectangle->setArrowWidth(18);
        arrowRectangle->setArrowHeight(10);
        PopupWindow = arrowRectangle;
    }

    m_popupTipsDelayTimer->setInterval(500);
    m_popupTipsDelayTimer->setSingleShot(true);

    m_popupAdjustDelayTimer->setInterval(10);
    m_popupAdjustDelayTimer->setSingleShot(true);

    connect(m_popupTipsDelayTimer, &QTimer::timeout, this, &SystemTrayItem::showHoverTips);
    connect(m_popupAdjustDelayTimer, &QTimer::timeout, this, &SystemTrayItem::updatePopupPosition, Qt::QueuedConnection);

    grabGesture(Qt::TapAndHoldGesture);
}

SystemTrayItem::~SystemTrayItem()
{
    if (m_popupShown)
        popupWindowAccept();
}

QString SystemTrayItem::itemKeyForConfig()
{
    return m_itemKey;
}

void SystemTrayItem::setActive(const bool active)
{
    Q_UNUSED(active);
}

void SystemTrayItem::updateIcon()
{
    m_pluginInter->refreshIcon(m_itemKey);
}

const QImage SystemTrayItem::trayImage()
{
    return QImage();
}

void SystemTrayItem::sendClick(uint8_t mouseButton, int x, int y)
{
    Q_UNUSED(mouseButton);
    Q_UNUSED(x);
    Q_UNUSED(y);

    // do not process this callback
    // handle all mouse event in override mouse function
}

QWidget *SystemTrayItem::trayTipsWidget()
{
    return m_pluginInter->itemTipsWidget(m_itemKey);
}

QWidget *SystemTrayItem::trayPopupApplet()
{
    return m_pluginInter->itemPopupApplet(m_itemKey);
}

const QString SystemTrayItem::trayClickCommand()
{
    return m_pluginInter->itemCommand(m_itemKey);
}

const QString SystemTrayItem::contextMenu() const
{
    return m_pluginInter->itemContextMenu(m_itemKey);
}

void SystemTrayItem::invokedMenuItem(const QString &menuId, const bool checked)
{
    m_pluginInter->invokedMenuItem(m_itemKey, menuId, checked);
}

QWidget *SystemTrayItem::centralWidget() const
{
    return m_centralWidget;
}

void SystemTrayItem::detachPluginWidget()
{
    QWidget *widget = m_pluginInter->itemWidget(m_itemKey);
    if (widget)
        widget->setParent(nullptr);
}

bool SystemTrayItem::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::DevicePixelRatioChange:
        // Some external tray plugins cache their pixmap before the dock has
        // been associated with its final screen. Refresh once the effective
        // DPR is known so their icon is not painted at the startup DPR.
        QTimer::singleShot(0, this, &SystemTrayItem::updateIcon);
        break;
    default:
        break;
    }

    if (m_popupShown)
    {
        switch (event->type())
        {
        case QEvent::Paint:
            if (!m_popupAdjustDelayTimer->isActive())
                m_popupAdjustDelayTimer->start();
            break;
        default:;
        }
    }

    if (event->type() == QEvent::Gesture)
        gestureEvent(static_cast<QGestureEvent*>(event));

    return AbstractTrayWidget::event(event);
}

void SystemTrayItem::enterEvent(QEnterEvent *event)
{
    m_popupTipsDelayTimer->start();
    update();

    AbstractTrayWidget::enterEvent(event);
}

void SystemTrayItem::leaveEvent(QEvent *event)
{
    m_popupTipsDelayTimer->stop();

    // auto hide if popup is not model window
    if (m_popupShown && !PopupWindow->model())
        hidePopup();

    update();

    AbstractTrayWidget::leaveEvent(event);
}

void SystemTrayItem::mousePressEvent(QMouseEvent *event)
{
    m_popupTipsDelayTimer->stop();
    hideNonModel();

    if (event->button() == Qt::RightButton) {
        if (perfectIconRect().contains(event->pos())) {
            return showContextMenu();
        }
    }

    AbstractTrayWidget::mousePressEvent(event);
}

void SystemTrayItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (checkAndResetTapHoldGestureState()&& event->source() == Qt::MouseEventSynthesizedByQt) {
        qDebug() << "SystemTray: tap and hold gesture detected, ignore the synthesized mouse release event";
        return;
    }

    event->accept();

    showPopupApplet(trayPopupApplet());

    if (!trayClickCommand().isEmpty()) {
        QProcess::startDetached(trayClickCommand());
    }

    AbstractTrayWidget::mouseReleaseEvent(event);
}

const QPoint SystemTrayItem::popupMarkPoint() const
{
    QPoint p(topleftPoint());

    const QRect r = rect();
    const QRect wr = window()->rect();

    switch (DockPosition) {
    case Dock::Position::Top:
        p += QPoint(r.width() / 2, r.height() + (wr.height() - r.height()) / 2);
        break;
    case Dock::Position::Bottom:
        p += QPoint(r.width() / 2, 0 - (wr.height() - r.height()) / 2);
        break;
    case Dock::Position::Left:
        p += QPoint(r.width() + (wr.width() - r.width()) / 2, r.height() / 2);
        break;
    case Dock::Position::Right:
        p += QPoint(0 - (wr.width() - r.width()) / 2, r.height() / 2);
        break;
    }

    return p;
}

// 获取在最外层的窗口(MainWindow)中的位置
const QPoint SystemTrayItem::topleftPoint() const
{
    if (QGuiApplication::platformName().contains("wayland",
            Qt::CaseInsensitive)) {
        QWidget* topLevel = window();
        QScreen* screen = topLevel->screen();
        if (!screen) {
            screen = qApp->primaryScreen();
        }

        if (screen) {
            const QRect screenRect = screen->geometry();
            const QSize dockSize = topLevel->size();
            QPoint dockOrigin;

            switch (DockPosition) {
                case Dock::Position::Top:
                    dockOrigin = QPoint(
                        screenRect.left() + (screenRect.width() - dockSize.width()) / 2,
                        screenRect.top());
                    break;
                case Dock::Position::Bottom:
                    dockOrigin = QPoint(
                        screenRect.left() + (screenRect.width() - dockSize.width()) / 2,
                        screenRect.bottom() - dockSize.height() + 1);
                    break;
                case Dock::Position::Left:
                    dockOrigin = QPoint(
                        screenRect.left(),
                        screenRect.top() + (screenRect.height() - dockSize.height()) / 2);
                    break;
                case Dock::Position::Right:
                    dockOrigin = QPoint(
                        screenRect.right() - dockSize.width() + 1,
                        screenRect.top() + (screenRect.height() - dockSize.height()) / 2);
                    break;
            }

            return dockOrigin + mapTo(topLevel, QPoint());
        }
    }

    QPoint p;
    const QWidget *w = this;
    do {
        p += w->pos();
        w = qobject_cast<QWidget *>(w->parent());
    } while (w);

    return p;
}

void SystemTrayItem::hidePopup()
{
    m_popupTipsDelayTimer->stop();
    m_popupAdjustDelayTimer->stop();
    m_popupShown = false;
    PopupWindow->hide();

    emit PopupWindow->accept();
    emit requestWindowAutoHide(true);
}

void SystemTrayItem::hideNonModel()
{
    // auto hide if popup is not model window
    if (m_popupShown && !PopupWindow->model())
        hidePopup();
}

void SystemTrayItem::popupWindowAccept()
{
    if (!PopupWindow->isVisible())
        return;

    disconnect(PopupWindow.data(), &DockPopupWindow::accept, this, &SystemTrayItem::popupWindowAccept);

    hidePopup();
}

void SystemTrayItem::showPopupApplet(QWidget * const applet)
{
    // another model popup window already exists
    if (PopupWindow->model())
        return;

    if (!applet) {
        return;
    }

    showPopupWindow(applet, true);
}

void SystemTrayItem::showPopupWindow(QWidget * const content, const bool model)
{
    m_popupShown = true;
    m_lastPopupWidget = content;

    if (model)
        emit requestWindowAutoHide(false);

    DockPopupWindow *popup = PopupWindow.data();
    QScreen *targetScreen = window() ? window()->screen() : nullptr;
    if (targetScreen && popup->screen() != targetScreen)
        popup->setScreen(targetScreen);

    QWidget *lastContent = popup->getContent();
    if (lastContent)
        lastContent->setVisible(false);

    switch (DockPosition)
    {
    case Dock::Position::Top:   popup->setArrowDirection(DockPopupWindow::ArrowTop);     break;
    case Dock::Position::Bottom:popup->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Dock::Position::Left:  popup->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Dock::Position::Right: popup->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }
    content->resize(content->sizeHint());
    popup->setContent(content);

    QPoint p = popupMarkPoint();
    if (!popup->isVisible())
        QMetaObject::invokeMethod(popup, "show", Qt::QueuedConnection, Q_ARG(QPoint, p), Q_ARG(bool, model));
    else
        popup->show(p, model);

    connect(popup, &DockPopupWindow::accept, this, &SystemTrayItem::popupWindowAccept, Qt::UniqueConnection);
}

void SystemTrayItem::showHoverTips()
{
    // another model popup window already exists
    if (PopupWindow->model())
        return;

    // if not in geometry area
    if (!QGuiApplication::platformName().contains("wayland",
            Qt::CaseInsensitive)) {
        const QRect r(topleftPoint(), size());
        if (!r.contains(QCursor::pos()))
            return;
    }

    QWidget * const content = trayTipsWidget();
    if (!content)
        return;

    showPopupWindow(content);
}

/*!
 * \sa DockItem::checkAndResetTapHoldGestureState
 */
bool SystemTrayItem::checkAndResetTapHoldGestureState()
{
    bool ret = m_tapAndHold;
    m_tapAndHold = false;
    return ret;
}

void SystemTrayItem::gestureEvent(QGestureEvent *event)
{
    if (!event)
        return;

    QGesture *gesture = event->gesture(Qt::TapAndHoldGesture);

    if (!gesture)
        return;

    qDebug() << "SystemTray: got TapAndHoldGesture";

    m_tapAndHold = true;
}

void SystemTrayItem::showContextMenu()
{
    const QString menuJson = contextMenu();
    if (menuJson.isEmpty())
        return;

    if (QGuiApplication::platformName().contains("wayland",
                                                 Qt::CaseInsensitive)) {
        popupMenuWayland(menuJson);
        return;
    }

    QDBusPendingReply<QDBusObjectPath> result = m_menuManagerInter->RegisterMenu();

    result.waitForFinished();
    if (result.isError())
    {
        qWarning() << result.error();
        return;
    }

    const QPoint p = popupMarkPoint();

    QJsonObject menuObject;
    menuObject.insert("x", QJsonValue(p.x()));
    menuObject.insert("y", QJsonValue(p.y()));
    menuObject.insert("isDockMenu", QJsonValue(true));
    menuObject.insert("menuJsonContent", QJsonValue(menuJson));
    switch (DockPosition)
    {
    case Dock::Position::Top:       menuObject.insert("direction", "top");      break;
    case Dock::Position::Bottom:    menuObject.insert("direction", "bottom");   break;
    case Dock::Position::Left:      menuObject.insert("direction", "left");     break;
    case Dock::Position::Right:     menuObject.insert("direction", "right");    break;
    }

    const QDBusObjectPath path = result.argumentAt(0).value<QDBusObjectPath>();
    DBusMenu *menuInter = new DBusMenu(path.path(), this);

    connect(menuInter, &DBusMenu::ItemInvoked, this, &SystemTrayItem::invokedMenuItem);
    connect(menuInter, &DBusMenu::ItemInvoked, menuInter, &DBusMenu::deleteLater);
    connect(menuInter, &DBusMenu::MenuUnregistered, this, &SystemTrayItem::onContextMenuAccepted, Qt::QueuedConnection);

    menuInter->ShowMenu(QString(QJsonDocument(menuObject).toJson()));

    hidePopup();
    emit requestWindowAutoHide(false);
}

void SystemTrayItem::popupMenuWayland(const QString &menuJson)
{
    const QJsonArray items = QJsonDocument::fromJson(menuJson.toUtf8())
                                 .object().value("items").toArray();
    if (items.isEmpty())
        return;

    StyledMenu menu(QStringLiteral("ddark2"));

    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        QAction *action = menu.addAction(object.value("itemText").toString());
        action->setEnabled(object.value("isActive").toBool(true));
        action->setData(object.value("itemId").toString());
    }
    connect(&menu, &QMenu::triggered, &menu, [&](QAction *action) {
        if (action)
            invokedMenuItem(action->data().toString(), action->isChecked());
        menu.close();
    });

    hidePopup();
    emit requestWindowAutoHide(false);
    const QPoint menuPosition = popupMenuPosition(menu.sizeHint(), &menu);
    if (Wayland::LayerShellHelper::isWayland()) {
        Wayland::LayerShellHelper::preparePopupLayerShell(
            &menu, window() ? window()->screen() : nullptr, menuPosition);

        QScreen *menuScreen = window() ? window()->screen() : nullptr;
        if (!menuScreen)
            menuScreen = qApp->screenAt(menuPosition);
        if (!menuScreen)
            menuScreen = qApp->primaryScreen();

        QScopedPointer<MenuDismissMask> menuMask(new MenuDismissMask(&menu));
        menuMask->setScreen(menuScreen);
        Wayland::LayerShellHelper::setMenuMaskRole(menuMask.data());
        menuMask->show();

        menu.exec(menuPosition);
        menuMask->hide();
        onContextMenuAccepted();
        return;
    }

    menu.exec(menuPosition);
    onContextMenuAccepted();
}

void SystemTrayItem::onContextMenuAccepted()
{
    emit requestRefershWindowVisible();
    emit requestWindowAutoHide(true);
}

void SystemTrayItem::updatePopupPosition()
{
    Q_ASSERT(sender() == m_popupAdjustDelayTimer);

    if (!m_popupShown || !PopupWindow->model())
        return;

    if (PopupWindow->getContent() != m_lastPopupWidget.data())
        return popupWindowAccept();

    const QPoint p = popupMarkPoint();
    PopupWindow->show(p, PopupWindow->model());
}
