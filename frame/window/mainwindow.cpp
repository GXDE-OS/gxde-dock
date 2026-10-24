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

#include "mainwindow.h"
#include "controller/dockitemcontroller.h"
#include "panel/mainpanel.h"
#include "util/utils.h"
#include "wayland/layershellhelper.h"
#include "wayland/layershell_styler.h"
#include "wayland/gxdescreenshotclient.h"

#include <QDebug>
#include <QEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QLinearGradient>

#include <qpa/qplatformwindow.h>
#include <dapplication.h>

#include <DPlatformWindowHandle>

#include <X11/X.h>
#include <X11/Xutil.h>

#include "util/x11helper.h"

#define SNI_WATCHER_SERVICE "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_PATH "/StatusNotifierWatcher"

using org::kde::StatusNotifierWatcher;

const QPoint rawXPosition(const QPoint &scaledPos)
{
    QScreen const * screen = Utils::screenAtByScaled(scaledPos);

    return screen ? screen->geometry().topLeft() +
                        (scaledPos - screen->geometry().topLeft()) *
                            screen->devicePixelRatio()
                  : scaledPos;
}

const QPoint scaledPos(const QPoint &rawXPos)
{
    QScreen const * screen = Utils::screenAt(rawXPos);

    return screen
               ? screen->geometry().topLeft() +
                     (rawXPos - screen->geometry().topLeft()) / screen->devicePixelRatio()
               : rawXPos;
}

MainWindow::MainWindow(QScreen *screen, QWidget *parent)
    : QWidget(parent),

      m_launched(false),
      m_updatePanelVisible(false),
      m_screen(screen ? screen : qApp->primaryScreen()),
      m_itemController(DockItemController::instanceForScreen(m_screen, this)),
      m_mainPanel(new MainPanel(m_itemController, this)),

      m_platformWindowHandle(this),
      m_wmHelper(DWindowManagerHelper::instance()),

      m_positionUpdateTimer(new QTimer(this)),
      m_expandDelayTimer(new QTimer(this)),
      m_leaveDelayTimer(new QTimer(this)),
      m_shadowMaskOptimizeTimer(new QTimer(this)),

      m_sizeChangeAni(new QVariantAnimation(this)),
      m_posChangeAni(new QVariantAnimation(this)),
      m_panelShowAni(new QPropertyAnimation(m_mainPanel, "pos")),
      m_panelHideAni(new QPropertyAnimation(m_mainPanel, "pos")),
      m_xcbMisc(XcbMisc::instance()),
      m_dbusDaemonInterface(QDBusConnection::sessionBus().interface()),
      m_sniWatcher(new StatusNotifierWatcher(SNI_WATCHER_SERVICE, SNI_WATCHER_PATH, QDBusConnection::sessionBus(), this))
{
    setAccessibleName("dock-mainwindow");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setAcceptDrops(true);

    const bool wayland = Wayland::LayerShellHelper::isWayland();

    if (!wayland) {
        DPlatformWindowHandle::enableDXcbForWindow(this, true);
        m_platformWindowHandle.setEnableBlurWindow(false);
        m_platformWindowHandle.setTranslucentBackground(true);
        m_platformWindowHandle.setWindowRadius(0);
        m_platformWindowHandle.setBorderWidth(0);
        m_platformWindowHandle.setShadowOffset(QPoint(0, 0));
        m_platformWindowHandle.setShadowRadius(0);
    }

    m_settings = &DockSettings::Instance();

    if (wayland) {
        // layer-shell-qt在创建表面时固定窗体尺寸，即使后期resize也不会重新发送set_size
        // 所以初始化之前就得确定真实高度了
        // 不然的话dock的尺寸会卡在100*30 在桌面上看就是半个dock
        resize(surfaceSize(m_settings->windowSize(m_screen), m_settings->position()));
        Wayland::LayerShellHelper::setDockRole(this, m_screen,
            QStringLiteral("dde-shell/dock"), m_settings->position());
    } else {
        m_xcbMisc->set_window_type(winId(), XcbMisc::Dock);
    }

    initSNIHost();
    initComponents();
    initConnections();

    if (wayland) {
        GxdeScreenshotClient::instance()->refreshToplevels();
    }

    m_mainPanel->setFixedSize(m_settings->panelSize());
    m_mainPanel->installEventFilter(this);

    show();
}

MainWindow::~MainWindow()
{
    delete m_xcbMisc;
}

QScreen *MainWindow::screen() const
{
    return m_screen;
}

void MainWindow::launch()
{
    const bool wayland = Wayland::LayerShellHelper::isWayland();

    m_updatePanelVisible = false;
    m_mainPanel->setVisible(false);
    resetPanelEnvironment(false);

    if (!wayland) {
        setVisible(false);
    }

    QTimer::singleShot(400, this, [&] {
        m_launched = true;
        m_mainPanel->setVisible(true);
        resetPanelEnvironment(false);
        updateGeometry();
        expand();
    });

    // set strut
    QTimer::singleShot(600, this, [&] {
        setStrutPartial();
    });

    // reset to right environment when animation finished
    QTimer::singleShot(800, this, [&] {
        m_updatePanelVisible = true;
        updatePanelVisible();
    });

    qApp->processEvents();
    if (!wayland) {
        QTimer::singleShot(300, this, &MainWindow::show);
    }
}

bool MainWindow::event(QEvent *e)
{
    switch (e->type())
    {
    case QEvent::Move:
        if (!e->spontaneous())
            QTimer::singleShot(1, this, &MainWindow::positionCheck);
        break;
    default:;
    }

    return QWidget::event(e);
}

void MainWindow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    if (!Wayland::LayerShellHelper::isWayland()) {
        m_platformWindowHandle.setEnableBlurWindow(false);
        m_platformWindowHandle.setShadowOffset(QPoint());
        m_platformWindowHandle.setShadowRadius(0);
    }

    if (m_screenChangeConn)
        QObject::disconnect(m_screenChangeConn);
    // X11 must rebuild _NET_WM_STRUT_PARTIAL after the output geometry changes.
    // Do not use that clear/reapply path on Wayland: clearStrutPartial() sets
    // the exclusive zone to zero temporarily, which lets the desktop-icons
    // layer surface expand into the dock area.  Some compositors do not shrink
    // it again after the positive zone is restored.
    m_screenChangeConn = connect(m_screen, &QScreen::geometryChanged,
            windowHandle(), [this] (const QRect &) {
        // QtWayland can temporarily invalidate a QScreen while outputs are
        // being rebuilt.  Sending a zero-sized layer-shell update during that
        // interval can make the compositor disconnect the entire client.
        if (!m_screen || !m_screen->geometry().isValid()
                || m_screen->geometry().isEmpty()
                || (Wayland::LayerShellHelper::isWayland()
                    && m_screen->name().isEmpty())) {
            return;
        }

        if (Wayland::LayerShellHelper::isWayland()) {
            updateGeometry();

            // Keep the reservation positive throughout the resize and commit
            // the value calculated from the new dock geometry in place.
            if (m_settings->hideMode() == Dock::KeepShowing) {
                const Position side = m_settings->position();
                const QSize size = m_settings->windowSize(m_screen);
                const int zone = (side == Position::Top || side == Position::Bottom)
                    ? size.height() : size.width();
                Wayland::LayerShellHelper::updateExclusiveZone(this, zone);
            }
            return;
        }

        m_positionUpdateTimer->start();
    });

    windowHandle()->setScreen(m_screen);
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    e->ignore();

    if (e->button() == Qt::RightButton)
        m_settings->showDockSettingsMenu(this, e->position().toPoint());
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
#ifdef QT_DEBUG
    case Qt::Key_Escape:        qApp->quit();       break;
#endif
    default:;
    }
}

void MainWindow::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);

    m_leaveDelayTimer->stop();
    if (m_settings->hideState() != Show && m_panelShowAni->state() != QPropertyAnimation::Running)
        m_expandDelayTimer->start();
}

void MainWindow::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_expandDelayTimer->stop();
    m_leaveDelayTimer->start();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);

    if (m_settings->hideState() != Show) {
        m_expandDelayTimer->start();
    }
}

void MainWindow::setFixedSize(const QSize &size)
{
    const QPropertyAnimation::State state = m_sizeChangeAni->state();

    if (state == QPropertyAnimation::Stopped && this->size() == size)
        return;

    if (state == QPropertyAnimation::Running)
        return m_sizeChangeAni->setEndValue(size);

    m_sizeChangeAni->setStartValue(this->size());
    m_sizeChangeAni->setEndValue(size);
    m_sizeChangeAni->start();
}

void MainWindow::internalAnimationMove(int x, int y)
{
    const QPropertyAnimation::State state = m_posChangeAni->state();
    const QPoint p = m_posChangeAni->endValue().toPoint();
    const QPoint tp = QPoint(x, y);

    if (state == QPropertyAnimation::Stopped && p == tp)
        return;

    if (state == QPropertyAnimation::Running && p != tp)
        return m_posChangeAni->setEndValue(QPoint(x, y));

    m_posChangeAni->setStartValue(pos());
    m_posChangeAni->setEndValue(tp);
    m_posChangeAni->start();
}

void MainWindow::initSNIHost()
{
    // registor dock as SNI Host on dbus
    // 多屏: 每个窗口都是独立 SNI host，服务名要唯一(用窗口地址做后缀)
    QDBusConnection dbusConn = QDBusConnection::sessionBus();
    m_sniHostService = QString("org.kde.StatusNotifierHost-%1-%2")
        .arg(qApp->applicationPid())
        .arg(reinterpret_cast<quintptr>(this), 0, 16);
    dbusConn.registerService(m_sniHostService);
    dbusConn.registerObject("/StatusNotifierHost", this);

    if (m_sniWatcher->isValid()) {
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    } else {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon is not exist for now!";
    }
}

void MainWindow::initComponents()
{
    m_positionUpdateTimer->setSingleShot(true);
    m_positionUpdateTimer->setInterval(20);
    m_positionUpdateTimer->start();

    m_expandDelayTimer->setSingleShot(true);
    m_expandDelayTimer->setInterval(m_settings->expandTimeout());

    m_leaveDelayTimer->setSingleShot(true);
    m_leaveDelayTimer->setInterval(m_settings->narrowTimeout());

    m_shadowMaskOptimizeTimer->setSingleShot(true);
    m_shadowMaskOptimizeTimer->setInterval(100);

    m_sizeChangeAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_posChangeAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_panelShowAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_panelHideAni->setEasingCurve(QEasingCurve::InOutCubic);

    QTimer::singleShot(1, this, &MainWindow::compositeChanged);
}

void MainWindow::compositeChanged()
{
    // Wayland 下始终有合成器；DTK 的 hasComposite() 依赖 X11 的
    // _NET_WM_CM_S0 选区，在 Wayland 上会误报 false，导致所有动画时长被置 0。
    // 因此在 Wayland 下强制视为有合成器，恢复启动/尺寸变化动画。
    const bool composite = Wayland::LayerShellHelper::isWayland() ? true : m_wmHelper->hasComposite();

// NOTE(justforlxz): On the sw platform, there is an unstable
// display position error, disable animation solution
#ifndef DISABLE_SHOW_ANIMATION
    const int duration = composite ? 300 : 0;
#else
    const int duration = 0;
#endif

    m_sizeChangeAni->setDuration(duration);
    m_posChangeAni->setDuration(duration);
    m_panelShowAni->setDuration(duration);
    m_panelHideAni->setDuration(duration);

    m_mainPanel->setComposite(composite);

    m_shadowMaskOptimizeTimer->start();
    m_positionUpdateTimer->start();
}

void MainWindow::internalMove(const QPoint &p)
{
    // 在Wayland上，layer-shell surface是由WM和锚设定控制的
    // internalMove在Wayland上会部分把窗体移动到屏幕外
    // Wayland上跳过，使用updateGeometry/setFixedSize代替
    if (Wayland::LayerShellHelper::isWayland()) {
        return;
    }

    const bool isHide = m_settings->hideState() == HideState::Hide && !testAttribute(Qt::WA_UnderMouse);
    const bool pos_adjust = m_settings->hideMode() != HideMode::KeepShowing &&
                             isHide &&
                             m_posChangeAni->state() == QVariantAnimation::Stopped;
    if (!pos_adjust)
        return QWidget::move(p);

    QPoint rp = rawXPosition(p);
    const auto ratio = devicePixelRatioF();

    const QRect &r = m_settings->primaryRawRect();
    switch (m_settings->position())
    {
    case Left:      rp.setX(r.x());             break;
    case Top:       rp.setY(r.y());             break;
    case Right:     rp.setX(r.right() - 1);     break;
    case Bottom:    rp.setY(r.bottom() - 1);    break;
    }

    int hx = height() * ratio, wx = width() * ratio;
    if (m_settings->hideMode() != HideMode::KeepShowing &&
        isHide &&
        m_panelHideAni->state() == QVariantAnimation::Stopped &&
        m_panelShowAni->state() == QVariantAnimation::Stopped)
    {
        switch (m_settings->position())
        {
        case Top:
        case Bottom:
            hx = 2;
            break;
        case Left:
        case Right:
            wx = 2;
        }
    }

    // using platform window to set real window position
    windowHandle()->handle()->setGeometry(QRect(rp.x(), rp.y(), wx, hx));
}

void MainWindow::initConnections()
{
    connect(m_settings, &DockSettings::dataChanged, m_positionUpdateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::positionChanged, this, &MainWindow::positionChanged);
    connect(m_settings, &DockSettings::autoHideChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowGeometryChanged, this, &MainWindow::updateGeometry, Qt::DirectConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, this, &MainWindow::setStrutPartial, Qt::QueuedConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, [this] { resetPanelEnvironment(true); });
    connect(m_settings, &DockSettings::windowHideModeChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowVisibleChanged, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_settings, &DockSettings::displayModeChanegd, m_positionUpdateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::displayModeChanegd, this, [this] {
        updateShadowRegions(true);
    });

    connect(m_settings, &DockSettings::v23ThemeChanged, this, static_cast<void (MainWindow::*)()>(&MainWindow::update));

    connect(m_mainPanel, &MainPanel::requestRefershWindowVisible, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_mainPanel, &MainPanel::requestWindowAutoHide, m_settings, &DockSettings::setAutoHide);
    connect(m_mainPanel, &MainPanel::geometryChanged, this, &MainWindow::panelGeometryChanged);
    connect(m_mainPanel, &MainPanel::v20CapsulesChanged, this, [this] { updateShadowRegions(); });
    connect(m_settings, &DockSettings::v20CapsuleChanged, this, &MainWindow::setStrutPartial, Qt::QueuedConnection);

    connect(m_positionUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePosition, Qt::QueuedConnection);
    connect(m_expandDelayTimer, &QTimer::timeout, this, &MainWindow::expand, Qt::QueuedConnection);
    connect(m_leaveDelayTimer, &QTimer::timeout, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_shadowMaskOptimizeTimer, &QTimer::timeout, this, &MainWindow::adjustShadowMask, Qt::QueuedConnection);

    connect(m_panelHideAni, &QPropertyAnimation::finished, this, &MainWindow::updateGeometry, Qt::QueuedConnection);
    connect(m_panelHideAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_panelShowAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_posChangeAni, &QVariantAnimation::valueChanged, this, static_cast<void (MainWindow::*)()>(&MainWindow::internalMove));
    connect(m_posChangeAni, &QVariantAnimation::finished, this, static_cast<void (MainWindow::*)()>(&MainWindow::internalMove), Qt::QueuedConnection);

    // to fix qt animation bug, sometimes window size not change
    connect(m_sizeChangeAni, &QVariantAnimation::valueChanged, [=](const QVariant &value) {
        QWidget::setFixedSize(value.toSize());
    });

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &MainWindow::compositeChanged, Qt::QueuedConnection);
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::frameMarginsChanged, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

    connect(m_dbusDaemonInterface, &QDBusConnectionInterface::serviceOwnerChanged, this, &MainWindow::onDbusNameOwnerChanged);
}

const QPoint MainWindow::x11GetWindowPos()
{
    const auto disp = x11Display();

    unsigned int unused;
    int x;
    int y;
    Window unused_window;

    XGetGeometry(disp, winId(), &unused_window, &x, &y, &unused, &unused, &unused, &unused);
    XFlush(disp);

    return QPoint(x, y);
}

void MainWindow::x11MoveWindow(const int x, const int y)
{
    const auto disp = x11Display();

    XMoveWindow(disp, winId(), x, y);
    XFlush(disp);
}

void MainWindow::x11MoveResizeWindow(const int x, const int y, const int w, const int h)
{
    const auto disp = x11Display();

    XMoveResizeWindow(disp, winId(), x, y, w, h);
    XFlush(disp);
}

void MainWindow::positionChanged(const Position prevPos)
{
    // paly hide animation and disable other animation
    m_updatePanelVisible = false;
    clearStrutPartial();
    narrow(prevPos);

    // reset position & layout and slide out
    QTimer::singleShot(200, this, [&] {
        resetPanelEnvironment(false, true);
        updateGeometry();
        expand();
    });

    // set strut
    QTimer::singleShot(400, this, [&] {
        setStrutPartial();
    });

    // reset to right environment when animation finished
    QTimer::singleShot(600, this, [&] {
        m_updatePanelVisible = true;
        updatePanelVisible();
    });
}

void MainWindow::updatePosition()
{
    // all update operation need pass by timer
    Q_ASSERT(sender() == m_positionUpdateTimer);

    clearStrutPartial();
    updateGeometry();

    // make sure strut partial is set after the size/position animation;
    const int duration = qMax(m_sizeChangeAni->duration(), m_posChangeAni->duration());

    QTimer::singleShot(duration, this, &MainWindow::setStrutPartial);
    QTimer::singleShot(duration, this, &MainWindow::updatePanelVisible);
}

void MainWindow::updateGeometry()
{
    const Position position = m_settings->position();
    QSize size = m_settings->windowSize(m_screen);

    if (Wayland::LayerShellHelper::isWayland()) {
        Wayland::LayerShellHelper::updateDockAnchor(this, position);
    }

    // DockDisplayMode and DockPosition MUST be set before invoke setFixedSize method of MainPanel
    m_mainPanel->updateDockDisplayMode(m_settings->displayMode());
    m_mainPanel->updateDockPosition(position);
    // this->setFixedSize has been overridden for size animation
    m_mainPanel->setFixedSize(size);

    bool animation = true;
    bool isHide = m_settings->hideState() == Hide && !testAttribute(Qt::WA_UnderMouse);

    if (isHide) {
        m_sizeChangeAni->stop();
        m_posChangeAni->stop();
        switch (position) {
        case Top:
        case Bottom:    size.setHeight(2);      break;
        case Left:
        case Right:     size.setWidth(2);       break;
        }
        animation = false;
        m_sizeChangeAni->setEndValue(size);
        QWidget::setFixedSize(size);
    } else {
        // this->setFixedSize has been overridden for size animation
        setFixedSize(surfaceSize(size, position));

        if (m_panelShowAni->state() != QPropertyAnimation::Running
                && m_panelHideAni->state() != QPropertyAnimation::Running) {
            m_mainPanel->move(panelOffset(position));
        }
        updateShadowRegions(true);
    }

    const QRect windowRect = m_settings->windowRect(position, isHide, m_screen);

    if (animation)
        internalAnimationMove(windowRect.x(), windowRect.y());
    else
        internalMove(windowRect.topLeft());

    m_mainPanel->update();
    m_shadowMaskOptimizeTimer->start();
}

void MainWindow::clearStrutPartial()
{
    if (Wayland::LayerShellHelper::isWayland()) {
        Wayland::LayerShellHelper::updateExclusiveZone(this, 0);
        return;
    }

    m_xcbMisc->clear_strut_partial(winId());
}

void MainWindow::setStrutPartial()
{
    // first, clear old strut partial
    clearStrutPartial();

    // reset env
    resetPanelEnvironment(true);

    if (m_settings->hideMode() != Dock::KeepShowing)
        return;


    // Wayland上保留区域是layer-shell设置的exclusive zone，WM负责处理
    // 所以我们放松就好~
    const int v20Gap = m_settings->v20Margin();

    if (Wayland::LayerShellHelper::isWayland()) {
        const Position side = m_settings->position();
        const QSize s = m_settings->windowSize(m_screen);
        const int zone = (side == Position::Top || side == Position::Bottom)
            ? s.height() : s.width();
        Wayland::LayerShellHelper::updateExclusiveZone(this, zone + v20Gap);
        return;
    }

    const auto ratio = devicePixelRatioF();
    const int maxScreenHeight = m_settings->screenRawHeight();
    const int maxScreenWidth = m_settings->screenRawWidth();
    const Position side = m_settings->position();
    const QPoint &p = rawXPosition(m_posChangeAni->endValue().toPoint());
    const QSize &s = m_settings->windowSize(m_screen);
    const QRect &primaryRawRect = m_settings->primaryRawRect();
    const int gap = qRound(v20Gap * ratio);

    XcbMisc::Orientation orientation = XcbMisc::OrientationTop;
    uint strut = 0;
    uint strutStart = 0;
    uint strutEnd = 0;

    QRect strutArea(0, 0, maxScreenWidth, maxScreenHeight);
    switch (side)
    {
    case Position::Top:
        orientation = XcbMisc::OrientationTop;
        strut = p.y() + s.height() * ratio + gap;
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setBottom(strut);
        break;
    case Position::Bottom:
        orientation = XcbMisc::OrientationBottom;
        strut = maxScreenHeight - p.y() + gap;
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setTop(p.y() - gap);
        break;
    case Position::Left:
        orientation = XcbMisc::OrientationLeft;
        strut = p.x() + s.width() * ratio + gap;
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setRight(strut);
        break;
    case Position::Right:
        orientation = XcbMisc::OrientationRight;
        strut = maxScreenWidth - p.x() + gap;
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setLeft(p.x() - gap);
        break;
    default:
        Q_ASSERT(false);
    }

    qDebug() << "screen info: " << p << strutArea;

    // pass if strut area is intersect with other screen
    int count = 0;
    const QRect pr = m_settings->primaryRect();
    for (auto *screen : qApp->screens())
    {
        const QRect sr = screen->geometry();
        if (sr == pr)
            continue;

        if (sr.intersects(strutArea))
            ++count;
    }
    if (count > 0)
    {
        qWarning() << "strutArea is intersects with another screen.";
        qWarning() << maxScreenHeight << maxScreenWidth << side << p << s;
        return;
    }

    m_xcbMisc->set_strut_partial(winId(), orientation, strut, strutStart, strutEnd);
}

void MainWindow::expand()
{
    qApp->processEvents();

    const auto showAniState = m_panelShowAni->state();
    m_panelHideAni->stop();

    const QPoint finishPos = panelOffset(m_settings->position());

    resetPanelEnvironment(true, false);

    if (showAniState != QPropertyAnimation::Running && m_mainPanel->pos() != m_panelShowAni->currentValue())
    {
        QPoint startPos = finishPos;
        const QSize &size = m_settings->windowSize(m_screen);
        switch (m_settings->position())
        {
        case Top:       startPos.ry() -= size.height();     break;
        case Bottom:    startPos.ry() += size.height();     break;
        case Left:      startPos.rx() -= size.width();      break;
        case Right:     startPos.rx() += size.width();      break;
        }

        m_panelShowAni->setStartValue(startPos);
        m_panelShowAni->setEndValue(finishPos);
        m_panelShowAni->start();
        m_shadowMaskOptimizeTimer->start();
        m_platformWindowHandle.setShadowRadius(0);
    }
}

void MainWindow::narrow(const Position prevPos)
{
    const QSize size = m_settings->windowSize(m_screen);

    QPoint finishPos = panelOffset(prevPos);
    switch (prevPos)
    {
    case Top:       finishPos.ry() -= size.height();     break;
    case Bottom:    finishPos.ry() += size.height();     break;
    case Left:      finishPos.rx() -= size.width();      break;
    case Right:     finishPos.rx() += size.width();      break;
    }

    m_panelShowAni->stop();
    m_panelHideAni->setStartValue(m_mainPanel->pos());
    m_panelHideAni->setEndValue(finishPos);
    m_panelHideAni->start();
    m_platformWindowHandle.setShadowRadius(0);

    resetPanelEnvironment(false, false);
}

void MainWindow::resetPanelEnvironment(const bool visible, const bool resetPosition)
{
    if (!m_launched)
        return;

    // reset environment
    m_sizeChangeAni->stop();
    m_posChangeAni->stop();

    const Position position = m_settings->position();
    const QRect r(m_settings->windowRect(position, false, m_screen));

    m_sizeChangeAni->setEndValue(surfaceSize(r.size(), position));
    m_mainPanel->setFixedSize(m_settings->windowSize(m_screen));
    QWidget::setFixedSize(surfaceSize(r.size(), position));
    m_posChangeAni->setEndValue(r.topLeft());
    if (!Wayland::LayerShellHelper::isWayland()) {
        QWidget::move(r.topLeft());
    }

    if (!resetPosition)
        return;
    QPoint finishPos = panelOffset(position);
    switch (position)
    {
    case Top:       finishPos.ry() -= visible ? 0 : r.height();     break;
    case Bottom:    finishPos.ry() += visible ? 0 : r.height();     break;
    case Left:      finishPos.rx() -= visible ? 0 : r.width();      break;
    case Right:     finishPos.rx() += visible ? 0 : r.width();      break;
    }

    m_mainPanel->move(finishPos);
    m_mainPanel->updateDockPosition(position);
}

void MainWindow::updatePanelVisible()
{
    if (!m_updatePanelVisible)
        return;
    if (m_settings->hideMode() == KeepShowing)
        return expand();
    if (m_settings->hideMode() == KeepHidden)
        return narrow(m_settings->position());

    const Dock::HideState state = m_settings->hideState();

    do
    {
        if (state != Hide)
            break;

        if (!m_settings->autoHide())
            break;

        QRect r(pos(), size());
        if (r.contains(QCursor::pos()))
            break;

        return narrow(m_settings->position());

    } while (false);

    return expand();
}

void MainWindow::adjustShadowMask()
{
    if (!m_launched)
        return;

    if (m_shadowMaskOptimizeTimer->isActive())
        return;

    const bool composite = m_wmHelper->hasComposite();
    const bool isFasion = m_settings->displayMode() == Fashion;

    m_platformWindowHandle.setWindowRadius(composite && isFasion ? 5 : 0);
}

void MainWindow::positionCheck()
{
    if (m_posChangeAni->state() == QPropertyAnimation::Running)
        return;
    if (m_positionUpdateTimer->isActive())
        return;

    // 多屏: 用本窗口屏幕的期望矩形做校验
    const QPoint expectPos = m_settings->windowRect(m_settings->position(), false, m_screen).topLeft();

    if (QPoint(pos() - expectPos).manhattanLength() < 2)
        return;

    qWarning() << "Dock position may error!!!!!";
    qDebug() << pos() << expectPos << m_settings->windowRect(m_settings->position(), false, m_screen);

    // this may cause some position error and animation caton
    //internalMove();
}

void MainWindow::onDbusNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);

    if (name == SNI_WATCHER_SERVICE && !newOwner.isEmpty()) {
        qDebug() << SNI_WATCHER_SERVICE << "SNI watcher daemon started, register dock to watcher as SNI Host";
        m_sniWatcher->RegisterStatusNotifierHost(m_sniHostService);
    }
}

// flakewm draws no shadows for layer-shell surfaces, so in V23 mode the dock
// surface extends past the bar on the desktop side and the shadow is painted
// there.  The extra strip takes no input, no blur and no exclusive zone.
int MainWindow::shadowSize() const
{
    if (!Wayland::LayerShellHelper::isWayland() || m_settings->displayMode() != Dock::V23
            || Dock::isV20Mode())
        return 0;

    return 10;
}

QSize MainWindow::surfaceSize(const QSize &panelSize, const Position position) const
{
    const int shadow = shadowSize();

    if (position == Top || position == Bottom)
        return panelSize + QSize(0, shadow);

    return panelSize + QSize(shadow, 0);
}

QPoint MainWindow::panelOffset(const Position position) const
{
    switch (position) {
    case Bottom:    return QPoint(0, shadowSize());
    case Right:     return QPoint(shadowSize(), 0);
    default:        return QPoint();
    }
}

QRect MainWindow::panelRestRect() const
{
    const int shadow = shadowSize();
    QRect r = rect();

    switch (m_settings->position()) {
    case Top:       r.setBottom(r.bottom() - shadow);   break;
    case Bottom:    r.setTop(r.top() + shadow);         break;
    case Left:      r.setRight(r.right() - shadow);     break;
    case Right:     r.setLeft(r.left() + shadow);       break;
    }

    return r;
}

void MainWindow::updateShadowRegions(const bool forceStyle)
{
    QWindow *window = windowHandle();
    if (!window) {
        return;
    }

    if (!Wayland::LayerShellHelper::isWayland()) {
        QRegion region;
        if (Dock::isV20Mode() && m_wmHelper->hasComposite()) {
            for (const QRect &capsule : m_mainPanel->v20Capsules()) {
                const int radius = m_settings->v20CapsuleRadius(capsule);
                region += Wayland::LayerShellStyler::roundedRegion(
                    capsule.translated(m_mainPanel->pos()), radius);
            }
        }
        if (region != m_dockBlurRegion) {
            m_dockBlurRegion = region;
            window->setMask(region);
        }
        return;
    }

    // input region: only the bar itself, so the shadow strip never blocks
    // clicks on the windows below it
    const QRect bar = panelRestRect();
    const bool hasShadow = shadowSize() > 0 && !bar.isEmpty();
    QRegion region = hasShadow ? QRegion(bar) : QRegion();

    if (Dock::isV20Mode()) {
        for (const QRect &capsule : m_mainPanel->v20Capsules()) {
            const int radius = m_settings->v20CapsuleRadius(capsule);
            region += Wayland::LayerShellStyler::roundedRegion(
                capsule.translated(m_mainPanel->pos()), radius);
        }
    }

    window->setMask(region);

    if (forceStyle || region != m_dockBlurRegion) {
        m_dockBlurRegion = region;
        Wayland::LayerShellHelper::updateDockStyle(this, region);
    }
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    updateShadowRegions();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == m_mainPanel && (e->type() == QEvent::Move || e->type() == QEvent::Resize)) {
        update();
        if (Dock::isV20Mode())
            updateShadowRegions();
    }

    return QWidget::eventFilter(watched, e);
}

void MainWindow::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    const int shadow = shadowSize();
    if (!shadow || !m_mainPanel->isVisible())
        return;

    const QRect panel = m_mainPanel->geometry();
    QRect shadowRect;
    QLinearGradient gradient;
    switch (m_settings->position()) {
    case Top:
        shadowRect = QRect(panel.left(), panel.bottom() + 1, panel.width(), shadow);
        gradient = QLinearGradient(shadowRect.topLeft(), shadowRect.bottomLeft());
        break;
    case Bottom:
        shadowRect = QRect(panel.left(), panel.top() - shadow, panel.width(), shadow);
        gradient = QLinearGradient(shadowRect.bottomLeft(), shadowRect.topLeft());
        break;
    case Left:
        shadowRect = QRect(panel.right() + 1, panel.top(), shadow, panel.height());
        gradient = QLinearGradient(shadowRect.topLeft(), shadowRect.topRight());
        break;
    case Right:
        shadowRect = QRect(panel.left() - shadow, panel.top(), shadow, panel.height());
        gradient = QLinearGradient(shadowRect.topRight(), shadowRect.topLeft());
        break;
    }

    // a soft, gaussian-like falloff away from the bar
    const int alpha = m_settings->v23LightTheme() ? 18 : 40;
    gradient.setColorAt(0.0, QColor(0, 0, 0, alpha));
    gradient.setColorAt(0.25, QColor(0, 0, 0, alpha * 0.55));
    gradient.setColorAt(0.6, QColor(0, 0, 0, alpha * 0.15));
    gradient.setColorAt(1.0, QColor(0, 0, 0, 0));

    QPainter painter(this);
    painter.fillRect(shadowRect, gradient);
}
