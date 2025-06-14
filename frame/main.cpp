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

#include <DApplication>
#include <DLog>
#include <DDBusSender>

#include <QDir>

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
    DApplication::loadDXcbPlugin();
    DApplication app(argc, argv);
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
