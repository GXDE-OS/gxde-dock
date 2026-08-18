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

#include "docksettings.h"
#include "panel/mainpanel.h"
#include "dbus/dockdbusnames.h"
#include "item/appitem.h"
#include "util/utils.h"
#include "util/menudismissmask.h"
#include "wayland/layershellhelper.h"

#include <QDebug>
#include <QScopedPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QGSettings>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include <DApplication>
#include <QScreen>

#define ICON_SIZE_LARGE         48
#define ICON_SIZE_MEDIUM        36
#define ICON_SIZE_SMALL         30
#define FASHION_MODE_PADDING    30

#define DOCK_GSETTINGS_SCHEMA       "com.deepin.dde.dock"
#define DOCK_GSETTINGS_PATH         "/com/deepin/dde/dock/"
#define APPEARANCE_GSETTINGS_SCHEMA "com.deepin.dde.appearance"
#define APPEARANCE_GSETTINGS_PATH   "/com/deepin/dde/appearance/"

DWIDGET_USE_NAMESPACE

extern const QPoint rawXPosition(const QPoint &scaledPos);

namespace {

Position positionFromString(const QString& s) {
    if (s == "top") {
        return Top;
    } else if (s == "right") {
        return Right;
    } else if (s == "left") {
        return Left;
    } else {
        return Bottom;
    }
}

QString positionToString(const Position p) {
    switch (p) {
        case Top: {
            return "top";
        }

        case Right: {
            return "right";
        }

        case Left: {
            return "left";
        }

        case Bottom: {
            return "bottom";
        }

        default: {
            return "bottom";
        }
    }
}

DisplayMode displayModeFromString(const QString& s) {
    if (s == "fashion") {
        return Fashion;
    } else {
        return Efficient;
    }
}

QString displayModeToString(const DisplayMode m) {
    if (m == Fashion) {
        return "fashion";
    } else {
        return "efficient";
    }
}

HideMode hideModeFromString(const QString& s) {
    if (s == "keep-hidden") {
        return KeepHidden;
    } else if (s == "smart-hide") {
        return SmartHide;
    } else if (s == "auto-hide") {
        return AutoHide;
    } else {
        return KeepShowing;
    }
}

QString hideModeToString(const HideMode m) {
    switch (m) {
        case KeepHidden: {
            return "keep-hidden";
        }

        case SmartHide: {
            return "smart-hide";
        }

        case AutoHide: {
            return "auto-hide";
        }

        case KeepShowing: {
            return "keep-showing";
        }

        default: {
            return "keep-showing";
        }
    }
}

}  // namespace

DockSettings::DockSettings(QWidget *parent)
    : QObject(parent)
    , m_autoHide(true)
    , m_isMaxSize(false)
    , m_opacity(0.4)
    , m_fashionTraySize(QSize(0, 0))
    , m_fashionModeAct(tr("Fashion Mode"), this)
    , m_efficientModeAct(tr("Efficient Mode"), this)
    , m_topPosAct(tr("Top"), this)
    , m_bottomPosAct(tr("Bottom"), this)
    , m_leftPosAct(tr("Left"), this)
    , m_rightPosAct(tr("Right"), this)
    , m_largeSizeAct(tr("Large"), this)
    , m_mediumSizeAct(tr("Medium"), this)
    , m_smallSizeAct(tr("Small"), this)
    , m_keepShownAct(tr("Keep Shown"), this)
    , m_keepHiddenAct(tr("Keep Hidden"), this)
    , m_smartHideAct(tr("Smart Hide"), this)
    , m_systemMonitor(tr("System Monitor"), this)
    , m_windowSplit(tr("Window Split"), this)
    , m_displayInter(new DBusDisplay(DBusDisplay::staticServiceName(), this))
    , m_dockInter(new DBusDock(dockDBusService(), dockDBusManagerPath(), QDBusConnection::sessionBus(), this))
    , m_itemController(DockItemController::instance(this))
{
    m_daemonAvailable = dockDBusDaemonAvailable();
    m_dockGsettings = new QGSettings(DOCK_GSETTINGS_SCHEMA,
        DOCK_GSETTINGS_PATH, this);
    m_appearanceGsettings = new QGSettings(APPEARANCE_GSETTINGS_SCHEMA,
        APPEARANCE_GSETTINGS_PATH, this);

    updateScreenSize();
    m_position = currentPosition();
    m_displayMode = currentDisplayMode();
    m_hideMode = currentHideMode();
    m_hideState = m_daemonAvailable
        ? Dock::HideState(m_dockInter->hideState()) : Show;
    m_iconSize = currentIconSize();
    {
        const qreal ratio = dockRatio();
        AppItem::setIconBaseSize(m_iconSize * ratio, ratio);
    }
    DockItem::setDockPosition(m_position);
    qApp->setProperty(PROP_POSITION, QVariant::fromValue(m_position));
    DockItem::setDockDisplayMode(m_displayMode);
    qApp->setProperty(PROP_DISPLAY_MODE, QVariant::fromValue(m_displayMode));

    m_fashionModeAct.setCheckable(true);
    m_efficientModeAct.setCheckable(true);
    m_topPosAct.setCheckable(true);
    m_bottomPosAct.setCheckable(true);
    m_leftPosAct.setCheckable(true);
    m_rightPosAct.setCheckable(true);
    m_largeSizeAct.setCheckable(true);
    m_mediumSizeAct.setCheckable(true);
    m_smallSizeAct.setCheckable(true);
    m_keepShownAct.setCheckable(true);
    m_keepHiddenAct.setCheckable(true);
    m_smartHideAct.setCheckable(true);
    m_windowSplit.setCheckable(true);

    WhiteMenu *modeSubMenu = new WhiteMenu(&m_settingsMenu);
    modeSubMenu->addAction(&m_fashionModeAct);
    modeSubMenu->addAction(&m_efficientModeAct);
    QAction *modeSubMenuAct = new QAction(tr("Mode"), this);
    modeSubMenuAct->setMenu(modeSubMenu);
    modeSubMenuAct->setDisabled(QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-dock/mac-mode"));

    WhiteMenu *locationSubMenu = new WhiteMenu(&m_settingsMenu);
    locationSubMenu->addAction(&m_topPosAct);
    locationSubMenu->addAction(&m_bottomPosAct);
    locationSubMenu->addAction(&m_leftPosAct);
    locationSubMenu->addAction(&m_rightPosAct);
    QAction *locationSubMenuAct = new QAction(tr("Location"), this);
    locationSubMenuAct->setMenu(locationSubMenu);

    WhiteMenu *sizeSubMenu = new WhiteMenu(&m_settingsMenu);
    sizeSubMenu->addAction(&m_largeSizeAct);
    sizeSubMenu->addAction(&m_mediumSizeAct);
    sizeSubMenu->addAction(&m_smallSizeAct);
    QAction *sizeSubMenuAct = new QAction(tr("Size"), this);
    sizeSubMenuAct->setMenu(sizeSubMenu);

    WhiteMenu *statusSubMenu = new WhiteMenu(&m_settingsMenu);
    statusSubMenu->addAction(&m_keepShownAct);
    statusSubMenu->addAction(&m_keepHiddenAct);
    statusSubMenu->addAction(&m_smartHideAct);
    QAction *statusSubMenuAct = new QAction(tr("Status"), this);
    statusSubMenuAct->setMenu(statusSubMenu);

    m_hideSubMenu = new WhiteMenu(&m_settingsMenu);
    QAction *hideSubMenuAct = new QAction(tr("Plugins"), this);
    hideSubMenuAct->setMenu(m_hideSubMenu);

    m_settingsMenu.addAction(modeSubMenuAct);
    m_settingsMenu.addAction(locationSubMenuAct);
    m_settingsMenu.addAction(sizeSubMenuAct);
    m_settingsMenu.addAction(statusSubMenuAct);
    m_settingsMenu.addAction(hideSubMenuAct);
    m_settingsMenu.addAction(&m_windowSplit);
    // 需要确保安装了系统监视器才可显示
    if (QFile::exists("/usr/bin/deepin-system-monitor") || QFile::exists("/usr/bin/gxde-system-monitor")) {
        m_settingsMenu.addAction(&m_systemMonitor);
    }
    m_settingsMenu.setTitle("Settings Menu");

    connect(&m_settingsMenu, &WhiteMenu::triggered, this, &DockSettings::menuActionClicked);
    connect(m_dockInter, &DBusDock::PositionChanged, this, &DockSettings::onPositionChanged);
    connect(m_dockInter, &DBusDock::IconSizeChanged, this, &DockSettings::iconSizeChanged);
    connect(m_dockInter, &DBusDock::DisplayModeChanged, this, &DockSettings::onDisplayModeChanged);
    connect(m_dockInter, &DBusDock::HideModeChanged, this, &DockSettings::hideModeChanged, Qt::QueuedConnection);
    connect(m_dockInter, &DBusDock::HideStateChanged, this, &DockSettings::hideStateChanged);
    connect(m_dockInter, &DBusDock::ServiceRestarted, this, &DockSettings::resetFrontendGeometry);
    connect(m_dockInter, &DBusDock::OpacityChanged, this, &DockSettings::onOpacityChanged);

    // WindowSplit 变更后需要重启dock才能生效
    // deepin-daemon 在 WindowSplit 变更后重启自身进程使dock被SM重新拉起
    // gxde-daemon 不重启自身，因此前端需要自行重启
    connect(m_dockInter, &DBusDock::WindowSplitChanged, this, [this](bool) {
        qInfo() << "(Dock) WindowSplit changed, restarting dock...";
        QProcess::startDetached(QStringLiteral("/usr/bin/gxde-dock"), QStringList());
        qApp->quit();
    });

    connect(m_itemController, &DockItemController::itemInserted, this, &DockSettings::dockItemCountChanged, Qt::QueuedConnection);
    connect(m_itemController, &DockItemController::itemRemoved, this, &DockSettings::dockItemCountChanged, Qt::QueuedConnection);
    connect(m_itemController, &DockItemController::fashionTraySizeChanged, this, &DockSettings::onFashionTraySizeChanged, Qt::QueuedConnection);

    connect(m_displayInter, &DBusDisplay::PrimaryRectChanged, this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
    connect(m_displayInter, &DBusDisplay::ScreenHeightChanged, this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
    connect(m_displayInter, &DBusDisplay::ScreenWidthChanged, this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);

    // 监听QScreen几何变化，当com.deepin.daemon.Display不可用(如使用gxde-daemon)时
    // 也能正确响应屏幕分辨率变化
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged,
            this, [this](QScreen *newScreen) {
        if (newScreen) {
            connect(newScreen, &QScreen::geometryChanged,
                    this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
            connect(newScreen, &QScreen::logicalDotsPerInchChanged,
                    this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
    });
    if (qApp->primaryScreen()) {
        connect(qApp->primaryScreen(), &QScreen::geometryChanged,
                this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
        connect(qApp->primaryScreen(), &QScreen::geometryChanged,
                this, [this](const QRect &) {
            qInfo() << "(Dock) Screen geometry changed via QScreen";
        });
        connect(qApp->primaryScreen(), &QScreen::logicalDotsPerInchChanged,
                this, &DockSettings::primaryScreenChanged, Qt::QueuedConnection);
    }

    connect(&m_systemMonitor, &QAction::triggered, this, &DockSettings::openSystemMonitor);

    DApplication *app = qobject_cast<DApplication*>(qApp);
    if (app) {
        connect(app, &DApplication::iconThemeChanged, this, &DockSettings::gtkIconThemeChanged);
    }

    // Daemon不可用时使用GSettings做完备选方案获取位置/大小/模式等
    if (!m_daemonAvailable) {
        connect(m_dockGsettings, &QGSettings::changed,
          this, &DockSettings::onGsettingsChanged);
        connect(m_appearanceGsettings, &QGSettings::changed, this,
                [this](const QString &key) {
            if (key == "opacity") {
                onOpacityChanged(currentOpacity());
            }
        });
    }

    calculateWindowConfig();
    updateForbidPostions();
    resetFrontendGeometry();

    // X11下使用 DRegionMonitor 监听全局鼠标点击，用于右键菜单点击外部关闭
    // Qt6的QMenu::exec在dock的特殊窗口标志(Qt::Tool | Qt::WindowDoesNotAcceptFocus)下
    // 无法检测来自其他应用的点击事件
    if (!Wayland::LayerShellHelper::isWayland()) {
        m_menuRegionMonitor = new DRegionMonitor(this);
        m_menuRegionMonitor->setCoordinateType(DRegionMonitor::Original);
        connect(m_menuRegionMonitor, &DRegionMonitor::buttonPress,
                this, [this](const QPoint &mousePos, const int flag) {
            if (!m_menuRegionMonitor->registered())
                return;
            if (flag != DRegionMonitor::Button_Left && flag != DRegionMonitor::Button_Right)
                return;
            const QRect menuRect = QRect(m_settingsMenu.pos(), m_settingsMenu.size());
            if (!menuRect.contains(mousePos)) {
                m_menuRegionMonitor->unregisterRegion();
                QTimer::singleShot(0, this, [this]() {
                    m_settingsMenu.hide();
                });
            }
        });
    }

    QTimer::singleShot(0, this, [=] {onOpacityChanged(currentOpacity());});
}

DockSettings &DockSettings::Instance()
{
    static DockSettings settings;
    return settings;
}

void DockSettings::openSystemMonitor()
{
    QString program;
    if (QFile::exists("/usr/bin/gxde-system-monitor")) {
        program = "gxde-system-monitor";
    } else {
        program = "deepin-system-monitor";
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "dxcb;xcb");
    env.insert("QT_QPA_PLATFORMTHEME", "deepin");

    QProcess process;
    process.setProcessEnvironment(env);
    process.setProgram(program);
    process.startDetached();
}

// 使用Qt QScreen获取屏幕尺寸作为通用方案，同时保留DBus作为辅助
// 当com.deepin.daemon.Display不可用(如使用gxde-daemon)时，QScreen是唯一可靠来源
void DockSettings::updateScreenSize() {
    QScreen* s = qApp->primaryScreen();
    if (s) {
        const qreal dpr = s->devicePixelRatio();
        const QRect g = s->geometry();

        m_primaryRawRect = QRect(g.topLeft(),
            QSize(qRound(g.width() * dpr), qRound(g.height() * dpr)));
        m_screenRawWidth = m_primaryRawRect.width();
        m_screenRawHeight = m_primaryRawRect.height();
    }
}

const QRect DockSettings::primaryRect() const
{
    QRect rect = m_primaryRawRect;
    qreal scale = qApp->primaryScreen()->devicePixelRatio();

    rect.setWidth(std::round(qreal(rect.width()) / scale));
    rect.setHeight(std::round(qreal(rect.height()) / scale));

    return rect;
}

const QSize DockSettings::panelSize() const
{
    return m_mainWindowSize;
}

const QRect DockSettings::frontendWindowRect(QScreen *screen) const
{
    if (!screen)
        screen = qApp->primaryScreen();
    if (!screen)
        return QRect();

    const QRect logical = windowRect(m_position, false, screen);
    const qreal ratio = screen->devicePixelRatio();
    const QPoint rawTopLeft = rawXPosition(logical.topLeft());
    const QSize rawSize(qRound(logical.width() * ratio),
                        qRound(logical.height() * ratio));
    return QRect(rawTopLeft, rawSize);
}

const QRect DockSettings::windowRect(const Position position, const bool hide) const
{
    return windowRect(position, hide, qApp->primaryScreen());
}

// 多屏: 在指定屏幕上计算窗口矩形(主窗口尺寸全局一致，全宽模式的宽度按屏幕换算)
const QRect DockSettings::windowRect(const Position position, const bool hide, QScreen *screen) const
{
    if (!screen) {
        screen = qApp->primaryScreen();
    }

    QSize size = windowSize(screen);
    if (hide)
    {
        switch (position)
        {
        case Top:
        case Bottom:    size.setHeight(2);      break;
        case Left:
        case Right:     size.setWidth(2);       break;
        }
    }

    const QRect screenRect = screen->geometry();
    const int offsetX = (screenRect.width() - size.width()) / 2;
    const int offsetY = (screenRect.height() - size.height()) / 2;

    QPoint p(0, 0);
    switch (position)
    {
    case Top:
        p = QPoint(offsetX, 0);                                        break;
    case Left:
        p = QPoint(0, offsetY);                                        break;
    case Right:
        p = QPoint(screenRect.width() - size.width(), offsetY);    break;
    case Bottom:
        p = QPoint(offsetX, screenRect.height() - size.height());  break;
    default:Q_UNREACHABLE();
    }

    return QRect(screenRect.topLeft() + p, size);
}

// 多屏: 全局共享的窗口尺寸按屏幕换算。时尚模式的宽度按条目数计算，全宽模式取屏幕宽度。
const QSize DockSettings::windowSize(QScreen *screen) const
{
    QSize size = m_mainWindowSize;
    if (!screen) {
        return size;
    }

    // 主屏沿用全局尺寸（已按主屏计算）。副屏（非主屏）必须按其自身屏幕几何铺满，
    // 否则在副屏分辨率大于主屏时，任务栏尺寸会被钳制在主屏尺寸而显示不全。
    if (screen != qApp->primaryScreen()) {
        const QRect screenGeo = screen->geometry();
        switch (m_position) {
        case Top:
        case Bottom:
            size.setWidth(screenGeo.width());
            break;
        case Left:
        case Right:
            size.setHeight(screenGeo.height());
            break;
        default:
            break;
        }
        return size;
    }

    if (m_displayMode == Dock::Efficient || m_displayMode == Dock::Classic) {
        switch (m_position) {
        case Top:
        case Bottom:
            size.setWidth(screen->geometry().width());
            break;
        case Left:
        case Right:
            size.setHeight(screen->geometry().height());
            break;
        default:
            break;
        }
    }

    return size;
}

void DockSettings::showDockSettingsMenu(QWidget *sourceWindow,
                                        const QPoint &localPos)
{
    m_autoHide = false;

    QScreen *targetScreen = sourceWindow && sourceWindow->screen()
        ? sourceWindow->screen()
        : qApp->screenAt(QCursor::pos());
    if (!targetScreen) {
        targetScreen = qApp->primaryScreen();
    }
    m_menuItemController = DockItemController::instanceForScreen(targetScreen);

    // create actions
    QList<QAction *> actions;
    for (auto *p : m_menuItemController->pluginList())
    {
        if (!p->pluginIsAllowDisable())
            continue;

        const bool enable = !p->pluginIsDisable();
        const QString &name = p->pluginName();
        const QString &display = p->pluginDisplayName();

        // do not show trash in context menu under Efficient mode
        if (m_displayMode == Efficient && name == "trash") {
            continue;
        }

        QAction *act = new QAction(display, this);
        act->setCheckable(true);
        act->setChecked(enable);
        act->setData(name);

        actions << act;
    }

    // sort by name
    std::sort(actions.begin(), actions.end(), [] (QAction *a, QAction *b) -> bool {
        return a->data().toString() > b->data().toString();
    });

    // add actions
    qDeleteAll(m_hideSubMenu->actions());
    for (auto act : actions)
        m_hideSubMenu->addAction(act);

    m_fashionModeAct.setChecked(m_displayMode == Fashion);
    m_efficientModeAct.setChecked(m_displayMode == Efficient);
    m_topPosAct.setChecked(m_position == Top);
    m_topPosAct.setEnabled(!m_forbidPositions.contains(Top));
    m_bottomPosAct.setChecked(m_position == Bottom);
    m_bottomPosAct.setEnabled(!m_forbidPositions.contains(Bottom));
    m_leftPosAct.setChecked(m_position == Left);
    m_leftPosAct.setEnabled(!m_forbidPositions.contains(Left));
    m_rightPosAct.setChecked(m_position == Right);
    m_rightPosAct.setEnabled(!m_forbidPositions.contains(Right));
    m_largeSizeAct.setChecked(m_iconSize == ICON_SIZE_LARGE);
    m_mediumSizeAct.setChecked(m_iconSize == ICON_SIZE_MEDIUM);
    m_smallSizeAct.setChecked(m_iconSize == ICON_SIZE_SMALL);
    m_keepShownAct.setChecked(m_hideMode == KeepShowing);
    m_keepHiddenAct.setChecked(m_hideMode == KeepHidden);
    m_smartHideAct.setChecked(m_hideMode == SmartHide);
    m_windowSplit.setChecked(m_dockInter->windowSplit());

    // A parentless QMenu is incorrectly created as another layer surface by
    // layer-shell-qt.  Give the Wayland menu the Dock window as its transient
    // parent so it is attached as a real xdg_popup on the correct output.
    WhiteMenu waylandMenu;
    QMenu *popupMenu = &m_settingsMenu;
    if (Wayland::LayerShellHelper::isWayland()) {
        waylandMenu.addActions(m_settingsMenu.actions());
        waylandMenu.setTitle(m_settingsMenu.title());
        connect(&waylandMenu, &QMenu::triggered,
                this, &DockSettings::menuActionClicked);
        connect(&waylandMenu, &QMenu::triggered, &waylandMenu, &QMenu::close);
        popupMenu = &waylandMenu;
    }

    const QPoint menuPos = Wayland::LayerShellHelper::isWayland()
        ? popupMenuPosition(sourceWindow, popupMenu->sizeHint(), localPos,
                            popupMenu)
        : adjustMenuPos(popupMenu->sizeHint(), targetScreen,
                        sourceWindow ? sourceWindow->mapToGlobal(localPos)
                                     : QCursor::pos());

    if (Wayland::LayerShellHelper::isWayland()) {
        Wayland::LayerShellHelper::preparePopupLayerShell(
            popupMenu, targetScreen, menuPos);
    }

    // Wayland 的 QMenu 被改造成独立 layer-shell 表面后，不再自动拥有
    // xdg_popup 的“点击外部关闭”行为。这里垫一层全屏透明遮罩，
    // 点击菜单外任意位置时由 MenuDismissMask 主动关闭菜单。
    QScopedPointer<MenuDismissMask> waylandMenuMask;
    if (Wayland::LayerShellHelper::isWayland()) {
        waylandMenuMask.reset(new MenuDismissMask(popupMenu));
        waylandMenuMask->setScreen(targetScreen);
        Wayland::LayerShellHelper::setMenuMaskRole(waylandMenuMask.data());
        waylandMenuMask->show();
    }

    // X11下注册全局鼠标监听，使点击菜单外部时能关闭菜单
    if (m_menuRegionMonitor && !m_menuRegionMonitor->registered()) {
        m_menuRegionMonitor->setWatchedRegion(QRegion(targetScreen->geometry()));
        m_menuRegionMonitor->registerRegion();
    }

    popupMenu->exec(menuPos);

    // 菜单关闭后取消全局鼠标监听
    if (m_menuRegionMonitor && m_menuRegionMonitor->registered()) {
        m_menuRegionMonitor->unregisterRegion();
    }

    if (waylandMenuMask) {
        waylandMenuMask->hide();
    }

    setAutoHide(true);
}

QPoint DockSettings::popupMenuPosition(QWidget *sourceWindow,
                                       const QSize &menuSize,
                                       const QPoint &localAnchor,
                                       const QWidget *menu) const
{
    if (!sourceWindow)
        return QCursor::pos();

    QScreen *screen = sourceWindow->screen();
    if (!screen)
        screen = qApp->primaryScreen();

    const QMargins margins = menu ? menu->contentsMargins() : QMargins();
    const int panelWidth = menuSize.width() - margins.left() - margins.right();
    const int panelHeight = menuSize.height() - margins.top() - margins.bottom();

    QPoint local = localAnchor;
    switch (m_position) {
    case Top:
        local.setX(localAnchor.x() - margins.left() - panelWidth / 2);
        local.setY(sourceWindow->height() - margins.top());
        break;
    case Bottom:
        local.setX(localAnchor.x() - margins.left() - panelWidth / 2);
        local.setY(-menuSize.height() + margins.bottom());
        break;
    case Left:
        local.setX(sourceWindow->width() - margins.left());
        local.setY(localAnchor.y() - margins.top() - panelHeight / 2);
        break;
    case Right:
        local.setX(-menuSize.width() + margins.right());
        local.setY(localAnchor.y() - margins.top() - panelHeight / 2);
        break;
    }

    if (screen) {
        const QRect dockRect = windowRect(m_position, false, screen);
        const QRect screenLocal(screen->geometry().topLeft()
                                    - dockRect.topLeft(),
                                screen->geometry().size());
        local.setX(qBound(screenLocal.left(), local.x(),
                          screenLocal.right() - menuSize.width() + 1));
        local.setY(qBound(screenLocal.top(), local.y(),
                          screenLocal.bottom() - menuSize.height() + 1));
    }

    const QRect dockRect = screen
        ? windowRect(m_position, false, screen)
        : QRect(sourceWindow->pos(), sourceWindow->size());
    return dockRect.topLeft() + local;
}

QPoint DockSettings::adjustMenuPos(const QSize &menuSize, QScreen *screen,
                                   const QPoint &anchor) const
{
    if (!screen) {
        screen = qApp->screenAt(anchor);
    }
    if (!screen) {
        screen = qApp->primaryScreen();
    }

    const QRect dockRect = windowRect(m_position, false, screen);
    const QRect screenRect = screen->geometry();
    QPoint global = anchor;

    if (Wayland::LayerShellHelper::isWayland()) {
        switch (m_position) {
        case Top: {
            global.setY(dockRect.bottom());
            break;
        }

        case Bottom: {
            global.setY(dockRect.top() - menuSize.height());
            break;
        }

        case Left: {
            global.setX(dockRect.right());
            break;
        }

        case Right: {
            global.setX(dockRect.left() - menuSize.width());
            break;
        }
        }
    }

    global.setX(qBound(screenRect.left(), global.x(),
        screenRect.right() - menuSize.width() + 1));
    global.setY(qBound(screenRect.top(), global.y(),
        screenRect.bottom() - menuSize.height() + 1));
    return global;
}

void DockSettings::updateGeometry()
{

}

void DockSettings::setAutoHide(const bool autoHide)
{
    if (m_autoHide == autoHide)
        return;

    m_autoHide = autoHide;
    emit autoHideChanged(m_autoHide);
}

void DockSettings::menuActionClicked(QAction *action)
{
    Q_ASSERT(action);

    if (action == &m_fashionModeAct)
        return writeDisplayMode(Fashion);
    if (action == &m_efficientModeAct)
        return writeDisplayMode(Efficient);

    if (action == &m_topPosAct)
        return writePosition(Top);
    if (action == &m_bottomPosAct)
        return writePosition(Bottom);
    if (action == &m_leftPosAct)
        return writePosition(Left);
    if (action == &m_rightPosAct)
        return writePosition(Right);

    if (action == &m_largeSizeAct)
        return writeIconSize(ICON_SIZE_LARGE);
    if (action == &m_mediumSizeAct)
        return writeIconSize(ICON_SIZE_MEDIUM);
    if (action == &m_smallSizeAct)
        return writeIconSize(ICON_SIZE_SMALL);

    if (action == &m_keepShownAct)
        return writeHideMode(KeepShowing);
    if (action == &m_keepHiddenAct)
        return writeHideMode(KeepHidden);
    if (action == &m_smartHideAct)
        return writeHideMode(SmartHide);

    if (action == &m_windowSplit) {
        m_dockInter->setWindowSplit(m_windowSplit.isChecked());
        return;
    }

    // check plugin hide menu.
    const QString &data = action->data().toString();
    if (data.isEmpty())
        return;
    DockItemController *menuController = m_menuItemController
        ? m_menuItemController.data() : m_itemController;
    for (auto *p : menuController->pluginList())
    {
        if (p->pluginName() == data)
            return p->pluginStateSwitched();
    }
}

void DockSettings::onPositionChanged()
{
    const Position prevPos = m_position;
    const Position nextPos = currentPosition();

    if (prevPos == nextPos)
        return;

    emit positionChanged(prevPos);

    QTimer::singleShot(200, this, [this, nextPos] {
        m_position = nextPos;
        DockItem::setDockPosition(nextPos);
        qApp->setProperty(PROP_POSITION, QVariant::fromValue(nextPos));

        calculateWindowConfig();

        for (DockItemController *controller : DockItemController::instances()) {
            controller->refershItemsIcon();
        }
    });
}

void DockSettings::iconSizeChanged()
{
//    qDebug() << Q_FUNC_INFO;
    m_iconSize = currentIconSize();
    const qreal ratio = dockRatio();
    AppItem::setIconBaseSize(m_iconSize * ratio, ratio);

    calculateWindowConfig();

    emit dataChanged();
}

void DockSettings::onDisplayModeChanged()
{
//    qDebug() << Q_FUNC_INFO;
    m_displayMode = currentDisplayMode();
    DockItem::setDockDisplayMode(m_displayMode);
    qApp->setProperty(PROP_DISPLAY_MODE, QVariant::fromValue(m_displayMode));

    calculateWindowConfig();

    emit displayModeChanegd();

    for (DockItemController *controller : DockItemController::instances()) {
        QTimer::singleShot(1, controller, &DockItemController::sortPluginItems);
    }
}

void DockSettings::hideModeChanged()
{
//    qDebug() << Q_FUNC_INFO;
    m_hideMode = currentHideMode();

    emit windowHideModeChanged();
}

void DockSettings::hideStateChanged()
{
//    qDebug() << Q_FUNC_INFO;
    const Dock::HideState state = m_daemonAvailable
                                      ? Dock::HideState(m_dockInter->hideState())
                                      : m_hideState;

    if (state == Dock::Unknown)
        return;

    m_hideState = state;

    emit windowVisibleChanged();
}

void DockSettings::dockItemCountChanged()
{
    if (m_displayMode == Dock::Efficient)
        return;

    calculateWindowConfig();

    emit windowGeometryChanged();
}

void DockSettings::primaryScreenChanged()
{
//    qDebug() << Q_FUNC_INFO;
    updateScreenSize();

    calculateWindowConfig();
    updateForbidPostions();

    emit dataChanged();
}

void DockSettings::resetFrontendGeometry()
{
    m_frontendRect = frontendWindowRect(qApp->primaryScreen());
    m_dockInter->SetFrontendWindowRect(
        m_frontendRect.x(), m_frontendRect.y(),
        static_cast<uint>(m_frontendRect.width()),
        static_cast<uint>(m_frontendRect.height()));
}

bool DockSettings::test(const Position pos, const QList<QRect> &otherScreens) const
{
    QRect maxStrut(0, 0, m_screenRawWidth, m_screenRawHeight);
    switch (pos)
    {
    case Top:
        maxStrut.setBottom(m_primaryRawRect.top() - 1);
        maxStrut.setLeft(m_primaryRawRect.left());
        maxStrut.setRight(m_primaryRawRect.right());
        break;
    case Bottom:
        maxStrut.setTop(m_primaryRawRect.bottom() + 1);
        maxStrut.setLeft(m_primaryRawRect.left());
        maxStrut.setRight(m_primaryRawRect.right());
        break;
    case Left:
        maxStrut.setRight(m_primaryRawRect.left() - 1);
        maxStrut.setTop(m_primaryRawRect.top());
        maxStrut.setBottom(m_primaryRawRect.bottom());
        break;
    case Right:
        maxStrut.setLeft(m_primaryRawRect.right() + 1);
        maxStrut.setTop(m_primaryRawRect.top());
        maxStrut.setBottom(m_primaryRawRect.bottom());
        break;
    default:;
    }

    if (maxStrut.width() == 0 || maxStrut.height() == 0)
        return true;

    for (const auto &r : otherScreens)
        if (maxStrut.intersects(r))
            return false;

    return true;
}

void DockSettings::updateForbidPostions()
{
    qDebug() << Q_FUNC_INFO;

    const auto &screens = qApp->screens();
    if (screens.size() < 2)
        return m_forbidPositions.clear();

    QSet<Position> forbids;
    QList<QRect> rawScreenRects;
    for (auto *s : screens)
    {
        qInfo() << s->name() << s->geometry();

        if (s == qApp->primaryScreen())
            continue;

        const QRect &g = s->geometry();
        rawScreenRects << QRect(g.topLeft(), g.size() * s->devicePixelRatio());
    }

    qInfo() << rawScreenRects << m_screenRawWidth << m_screenRawHeight;

    if (!test(Top, rawScreenRects))
        forbids << Top;
    if (!test(Bottom, rawScreenRects))
        forbids << Bottom;
    if (!test(Left, rawScreenRects))
        forbids << Left;
    if (!test(Right, rawScreenRects))
        forbids << Right;

    m_forbidPositions = std::move(forbids);
}

void DockSettings::onOpacityChanged(const double value)
{
    if (m_opacity == value) return;

    m_opacity = value;

    emit opacityChanged(value * 255);
}

void DockSettings::onFashionTraySizeChanged(const QSize &traySize)
{
    if (m_displayMode == Dock::Efficient)
        return;

    if (m_fashionTraySize == traySize)
        return;

    m_fashionTraySize = traySize;

    calculateWindowConfig();

    emit windowGeometryChanged();
}

void DockSettings::calculateWindowConfig()
{
    const auto ratio = dockRatio();
    // 每次重算窗口配置时都用当前 ratio 刷新 IconBaseSize，
    // 避免 Wayland 启动阶段 DPR 从 2.0 修正到 1.25 后 IconBaseSize 仍是旧值，
    // 导致 adjustItemSize 里 itemSize 被算成负数/超大，图标乱飞、贴左上角。
    // 同时传入 ratio，保证 adjustItemSize 用来除以的 ratio 与此处设置时同源。
    AppItem::setIconBaseSize(m_iconSize * ratio, ratio);
    const int defaultHeight = std::round(AppItem::itemBaseHeight() / ratio);
    const int defaultWidth = std::round(AppItem::itemBaseWidth() / ratio);

    if (m_displayMode == Dock::Efficient || m_displayMode == Dock::Classic)
    {
        switch (m_position)
        {
        case Top:
        case Bottom:
            m_mainWindowSize.setHeight(defaultHeight + PANEL_BORDER);
            m_mainWindowSize.setWidth(primaryRect().width());
            break;

        case Left:
        case Right:
            m_mainWindowSize.setHeight(primaryRect().height());
            m_mainWindowSize.setWidth(defaultWidth + PANEL_BORDER);
            break;

        default:
            Q_ASSERT(false);
        }
    }
    else if (m_displayMode == Dock::Fashion)
    {
        int visibleItemCount = 0;
        const auto &itemList = m_itemController->itemList();
        for (auto item : itemList)
        {
            switch (item->itemType())
            {
            case DockItem::Launcher:
            case DockItem::App:
            case DockItem::Plugins:
            case DockItem::Placeholder:
                ++visibleItemCount;
                break;
            default:;
            }
        }

        const int perfectWidth = visibleItemCount * defaultWidth + PANEL_BORDER * 2 + PANEL_PADDING * 2 + PANEL_MARGIN * 2 + m_fashionTraySize.width();
        const int perfectHeight = visibleItemCount * defaultHeight + PANEL_BORDER * 2 + PANEL_PADDING * 2 + PANEL_MARGIN * 2 + m_fashionTraySize.height();
        const QRect &primaryRect = this->primaryRect();
        const int maxWidth = primaryRect.width() - FASHION_MODE_PADDING * 2;
        const int maxHeight = primaryRect.height() - FASHION_MODE_PADDING * 2;
        const int calcWidth = qMin(maxWidth, perfectWidth);
        const int calcHeight = qMin(maxHeight, perfectHeight);
        switch (m_position)
        {
        case Top:
        case Bottom: {
            m_mainWindowSize.setHeight(defaultHeight + PANEL_BORDER);
            m_mainWindowSize.setWidth(calcWidth);
            m_isMaxSize = (calcWidth == maxWidth);
            break;
        }
        case Left:
        case Right: {
            m_mainWindowSize.setHeight(calcHeight);
            m_mainWindowSize.setWidth(defaultWidth + PANEL_BORDER);
            m_isMaxSize = (calcHeight == maxHeight);
            break;
        }
        default:
            Q_ASSERT(false);
        }

        // used by FashionTrayItem of TrayPlugin
        qApp->setProperty("DockIsMaxiedSize", m_isMaxSize);
    } else {
        Q_ASSERT(false);
    }

    resetFrontendGeometry();
}

void DockSettings::gtkIconThemeChanged()
{
    qDebug() << Q_FUNC_INFO;
    m_itemController->refershItemsIcon();
}

qreal DockSettings::dockRatio() const
{
    // m_frontendRect 是物理(raw)坐标，必须用物理坐标判定的 screenAt 来匹配屏幕，
    // 不能误用 screenAtByScaled（它以逻辑 geometry 做 contains 判定）。
    // 双屏不同缩放下，若主屏 DPR 小于副屏，主屏物理坐标中心换算回逻辑坐标
    // 会落入副屏区域，screenAtByScaled 会误返回副屏 DPR，导致 IconBaseSize 被错误
    // 放大、主屏程序图标被算长（高效模式尤为明显）。
    QScreen const *screen = Utils::screenAt(m_frontendRect.center());

    return screen ? screen->devicePixelRatio() : qApp->devicePixelRatio();
}

Position DockSettings::currentPosition() const {
    if (m_daemonAvailable) {
        return Dock::Position(m_dockInter->position());
    } else {
        return positionFromString(m_dockGsettings->get("position").toString());
    }
}

DisplayMode DockSettings::currentDisplayMode() const {
    if (m_daemonAvailable) {
        return Dock::DisplayMode(m_dockInter->displayMode());
    } else {
        return displayModeFromString(m_dockGsettings->get("display-mode")
            .toString());
    }
}

HideMode DockSettings::currentHideMode() const {
    if (m_daemonAvailable) {
        return Dock::HideMode(m_dockInter->hideMode());
    } else {
        return hideModeFromString(m_dockGsettings->get("hide-mode")
            .toString());
    }
}

int DockSettings::currentIconSize() const {
    if (m_daemonAvailable) {
        return m_dockInter->iconSize();
    } else {
        return m_dockGsettings->get("icon-size").toInt();
    }
}

double DockSettings::currentOpacity() const {
    if (m_daemonAvailable) {
        return m_dockInter->opacity();
    } else {
        return m_appearanceGsettings->get("opacity").toDouble();
    }
}

void DockSettings::writePosition(const Position position) {
    if (m_daemonAvailable) {
        m_dockInter->setPosition(position);
    } else {
        m_dockGsettings->set("position", positionToString(position));
    }
}

void DockSettings::writeDisplayMode(const DisplayMode mode) {
    if (m_daemonAvailable) {
        m_dockInter->setDisplayMode(mode);
    } else {
        m_dockGsettings->set("display-mode", displayModeToString(mode));
    }
}

void DockSettings::writeHideMode(const HideMode mode) {
    if (m_daemonAvailable) {
        m_dockInter->setHideMode(mode);
    } else {
        m_dockGsettings->set("hide-mode", hideModeToString(mode));
    }
}

void DockSettings::writeIconSize(const int size) {
    if (m_daemonAvailable) {
        m_dockInter->setIconSize(size);
    } else {
        m_dockGsettings->set("icon-size", size);
    }
}

void DockSettings::onGsettingsChanged() {
    onPositionChanged();
    onDisplayModeChanged();
    hideModeChanged();
    iconSizeChanged();
}
