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

#include "window/mainwindow.h"
#include "util/themeappicon.h"
#include "wayland/layershellhelper.h"
#include "wayland/xsettings.h"

#include <DApplication>
#include <DLog>
#include <DDBusSender>

#include <QDir>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QIcon>
#include <QSettings>
#include <QApplication>
#include <QWidget>
#include <QWindow>

#include <LayerShellQt/Shell>

#include <unistd.h>
#include "dbus/dbusdockadaptors.h"
DWIDGET_USE_NAMESPACE
#ifdef DCORE_NAMESPACE
DCORE_USE_NAMESPACE
#else
DUTIL_USE_NAMESPACE
#endif

// let startdde know that we've already started.
void RegisterDdeSession()
{
    QString envName("DDE_SESSION_PROCESS_COOKIE_ID");

    QByteArray cookie = qgetenv(envName.toUtf8().data());
    qunsetenv(envName.toUtf8().data());

    if (!cookie.isEmpty())
    {
        QDBusPendingReply<bool> r = DDBusSender()
                .interface("com.deepin.SessionManager")
                .path("/com/deepin/SessionManager")
                .service("com.deepin.SessionManager")
                .method("Register")
                .arg(QString(cookie))
                .call();

        qDebug() << Q_FUNC_INFO << r.value();
    }
}

int main(int argc, char *argv[])
{
    // 以前dock不支持原生Wayland所以在Wayland下加D-XCB
    // 现在我们支持原生Wayland以后，启动时就不需要D-XCB了
    // 等layershell设置好后再把D-XCB弄回来，这样不支持Wayland的子进程就吃D-XCB了
    const QByteArray savedDtk2XWayland = qgetenv("DTK2_XWAYLAND");
    const bool waylandSession = qgetenv("XDG_SESSION_TYPE") == "wayland";

    if (waylandSession) {
        qDebug() << "Detected Wayland session!!";
        qunsetenv("DTK2_XWAYLAND");
        qputenv("QT_QPA_PLATFORM", "wayland");
        LayerShellQt::Shell::useLayerShell();
    } else {
        DApplication::loadDXcbPlugin();
    }

    DApplication app(argc, argv);

    if (waylandSession) {
        // 默认情况下，图标是从XSETTINGS的Net/IconThemeName读出来的
        // 拿不到的话从qt-theme读取图标主题，拿这个兜底
        QString iconTheme = Wayland::xsettingsString(QStringLiteral(
            "Net/IconThemeName"));

        // 如果没成功从XSETTINGS拿到...
        if (iconTheme.isEmpty()) {
            QSettings qtSettings(QSettings::IniFormat, QSettings::UserScope,
                "deepin", "qt-theme");
            qtSettings.beginGroup("Theme");
            iconTheme = qtSettings.value("IconThemeName").toString();
        }

        if (!iconTheme.isEmpty()) {
            QIcon::setThemeName(iconTheme);
        }

        // useLayerShell会把所有窗体变成layer-shell surface
        // 这下弹出菜单什么的都得糊满屏幕了
        // 为了防止这种情况发生弄一个小助手单独为菜单之类的设置锚点
        //
        // (仅 Treeland): 带子菜单的项(位置/大小/状态/插件)hover 出子菜单后继续下滑,
        // 整条菜单会消失。注意到子菜单关闭时, QtWayland 会在同一同步批次给父菜单误发一个
        // QEvent::Close,把整条菜单关掉(全程没有 FocusOut)
        // 处理方案: 当「刚有子菜单关闭」且「鼠标仍在父菜单矩形内」时,吞掉父菜单
        // Close; 如果是点外部→鼠标在菜单外或者直接选项→没有刚关的子菜单则视为关闭意图
        class PopupLayerShellPatcher : public QObject {
        public:
            using QObject::QObject;

        protected:
            bool eventFilter(QObject* object, QEvent* event) override {
                QWidget* target = qobject_cast<QWidget*>(object);
                if (target && target->windowType() == Qt::Popup) {
                    QWindow* wh = target->windowHandle();
                    const bool isSubMenu = wh && wh->transientParent();

                    if (event->type() == QEvent::Show) {
                        Wayland::LayerShellHelper::fixPopupLayerShell(target);
                    } else if ((event->type() == QEvent::Close
                                || event->type() == QEvent::Hide)
                               && isSubMenu) {
                        // 记下子菜单刚关闭的时刻, 用于识别误发Close
                        m_subMenuClosedTimer.restart();
                    } else if (event->type() == QEvent::Close && !isSubMenu
                               && Wayland::LayerShellHelper::isTreeland()) {
                        const bool subMenuJustClosed =
                            m_subMenuClosedTimer.isValid()
                            && m_subMenuClosedTimer.elapsed() < 50;
                        const bool cursorInsideMenu =
                            target->geometry().contains(QCursor::pos());
                        if (subMenuJustClosed && cursorInsideMenu) {
                            // 必须使用ignore()把QCloseEvent标为未接受, 这样
                            // QWidget::close() 才会放弃隐藏
                            event->ignore();
                            return true;
                        }  // 如果是false positive
                    }
                }
                return QObject::eventFilter(object, event);
            }

        private:
            QElapsedTimer m_subMenuClosedTimer;
        };

        app.installEventFilter(new PopupLayerShellPatcher(&app));
    }

    app.setOrganizationName("deepin");
    app.setApplicationName("gxde-dock");
    app.setApplicationDisplayName("DDE Dock");
    app.setApplicationVersion("2.0");
    app.setTheme("dark");
    app.loadTranslator();
    app.setAttribute(Qt::AA_EnableHighDpiScaling, true);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, false);

    // load gxde-network-utils translator
    QTranslator translator;
    translator.load("/usr/share/gxde-network-utils/translations/gxde-network-utils_" + QLocale::system().name());
    app.installTranslator(&translator);

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    QCommandLineOption disablePlugOption(QStringList() << "x" << "disable-plugins", "do not load plugins.");
    QCommandLineParser parser;
    parser.setApplicationDescription("DDE Dock");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(disablePlugOption);
    parser.process(app);

    if (!app.setSingleInstance(QString("gxde-dock_%1").arg(getuid()))) {
        qDebug() << "set single instance failed!!!!";
        return -1;
    }

    qDebug() << "\n\ngxde-dock startup";
    RegisterDdeSession();

#ifndef QT_DEBUG
    QDir::setCurrent(QApplication::applicationDirPath());
#endif
    MainWindow mw;

    if (waylandSession) {
        // 等layershell设置好后再把D-XCB弄回来，这样不支持Wayland的子进程就吃D-XCB了
        if (!savedDtk2XWayland.isEmpty()) {
            qputenv("DTK2_XWAYLAND", savedDtk2XWayland);
        }

        // 取消layer-shell集成，不然子进程自带layer-shell集成照样糊满屏幕
        qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
        qunsetenv("QT_QPA_PLATFORM");
    }

    if (QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-dock/dock-hide")) {
        return app.exec();
    }

    DBusDockAdaptors adaptor(&mw);
    QDBusConnection::sessionBus().registerService("com.deepin.dde.Dock");
    QDBusConnection::sessionBus().registerObject("/com/deepin/dde/Dock", "com.deepin.dde.Dock", &mw);

    QTimer::singleShot(1, &mw, &MainWindow::launch);

    if (QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-dock/mac-mode")) {
        // Mac 模式下强制开启时尚模式
        QProcess process;
        process.start("gsettings set com.deepin.dde.dock display-mode 'fashion'");
        process.waitForStarted();
        process.waitForFinished();
        process.close();
    }

    if (!parser.isSet(disablePlugOption) &&
        !QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-dock/mac-mode")) {
        DockItemController::instance()->startLoadPlugins();

    }
    return app.exec();
}
