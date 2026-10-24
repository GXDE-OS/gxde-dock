// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// The tiles that dde-tray-loader (eye-comfort-mode), dde-grand-search,
// deepin-screen-recorder, dde-clipboard and deepin-system-monitor put in the
// quick panel of Deepin V23, driven through the services GXDE has.

#include "v23toolmodules.h"
#include "v23dbusobject.h"

#include <DDBusSender>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString LightTheme = QStringLiteral("deepin");
const QString DarkTheme = QStringLiteral("deepin-dark");

void callLater(const QString &service, const QString &path, const QString &interface, const QString &method,
               const QVariantList &args = QVariantList())
{
    // give the panel the time to close, it must not show on a screenshot
    QTimer::singleShot(200, [=] {
        QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
        message.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(message);
    });
}
}

V23ThemeModule::V23ThemeModule(QObject *parent)
    : V23QuickModule(parent)
    , m_appearance(new V23DBusObject(QDBusConnection::sessionBus(), "com.deepin.daemon.Appearance",
                                     "/com/deepin/daemon/Appearance", "com.deepin.daemon.Appearance", this))
    , m_available(false)
{
    connect(m_appearance, &V23DBusObject::propertiesChanged, this, [this](const QStringList &names) {
        if (names.contains("GtkTheme"))
            update();
    });
    auto updateAvailable = [this] {
        if (m_appearance->isValid() != m_available) {
            m_available = m_appearance->isValid();
            emit availableChanged();
        }
    };
    connect(m_appearance, &V23DBusObject::invalidated, this, updateAvailable);
    connect(m_appearance, &V23DBusObject::propertiesChanged, this, updateAvailable);
}

QString V23ThemeModule::displayName() const
{
    return tr("Theme");
}

bool V23ThemeModule::isAvailable() const
{
    return m_available;
}

bool V23ThemeModule::isDark() const
{
    return m_appearance->property("GtkTheme").toString().endsWith("-dark");
}

void V23ThemeModule::update()
{
    if (m_tile) {
        m_tile->setIcon(isDark() ? QStringLiteral("theme-dark") : QStringLiteral("theme-light"));
        m_tile->setDescription(isDark() ? tr("Dark") : tr("Light"));
    }

    const QString current = m_appearance->property("GtkTheme").toString();
    for (const QPointer<V23ListView> &list : m_pages) {
        if (!list)
            continue;

        list->clear();
        const QList<QPair<QString, QString>> themes {
            { LightTheme, tr("Light") },
            { DarkTheme, tr("Dark") },
        };
        for (const auto &theme : themes) {
            V23ListItem *item = new V23ListItem(list->widget());
            item->setIcon(V23Icon { theme.first == DarkTheme ? QStringLiteral("theme-dark") : QStringLiteral("theme-light") });
            item->setText(theme.second);
            item->setSelected(current == theme.first);
            list->listLayout()->addWidget(item);

            const QString id = theme.first;
            connect(item, &V23ListItem::clicked, this, [this, id] {
                m_appearance->call("Set", {QStringLiteral("gtk"), id});
            });
        }
    }
}

QWidget *V23ThemeModule::createTile(QWidget *parent)
{
    // the theme icon shows without a circle, it is no switch
    m_tile = new V23QuickTile(V23QuickTile::Large, parent);
    m_tile->setTitle(displayName());
    m_tile->setIconDisplayOnly(true);
    connect(m_tile, &V23QuickTile::iconClicked, this, &V23ThemeModule::requestPage);
    connect(m_tile, &V23QuickTile::clicked, this, &V23ThemeModule::requestPage);
    update();
    return m_tile;
}

QWidget *V23ThemeModule::createPage(QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    V23ListView *list = new V23ListView(300, page);
    m_pages << list;
    layout->addWidget(list);
    layout->addSpacing(V23Quick::PanelSpacing);

    V23ListItem *settings = new V23ListItem(page);
    settings->setText(tr("Personalization settings"));
    settings->setArrow(true);
    layout->addWidget(settings);
    connect(settings, &V23ListItem::clicked, this, [this] {
        emit requestHidePopup();
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("ShowModule"))
            .arg(QString("personalization"))
            .call();
    });

    update();
    return page;
}

V23ActionModule::V23ActionModule(const QString &key, const QString &displayName, const QString &icon,
                                 std::function<bool()> available, std::function<void()> action, QObject *parent)
    : V23QuickModule(parent)
    , m_key(key)
    , m_displayName(displayName)
    , m_icon(icon)
    , m_available(available)
    , m_action(action)
{
}

QWidget *V23ActionModule::createTile(QWidget *parent)
{
    m_tile = new V23QuickTile(V23QuickTile::Standard, parent);
    m_tile->setIcon(m_icon);
    m_tile->setTitle(m_displayName);
    connect(m_tile, &V23QuickTile::clicked, this, [this] {
        emit requestHidePopup();
        m_action();
    });
    return m_tile;
}

void V23ActionModule::setActive(bool active, const QString &icon, V23Quick::IconMode mode)
{
    if (!m_tile)
        return;

    m_tile->setActive(active);
    m_tile->setIcon(icon.isEmpty() ? m_icon : icon, mode);
}

bool V23ActionModule::serviceAvailable(const QString &service)
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (bus->isServiceRegistered(service).value())
        return true;

    static QStringList activatable;
    if (activatable.isEmpty()) {
        const QDBusMessage reply = QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(
            "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames"));
        if (!reply.arguments().isEmpty())
            activatable = reply.arguments().first().toStringList();
    }

    return activatable.contains(service);
}

QList<V23QuickModule *> createActionModules(QObject *parent)
{
    QList<V23QuickModule *> modules;

    modules << new V23ActionModule(QStringLiteral("grand-search"), V23ThemeModule::tr("Grand Search"),
        QStringLiteral("search"),
        [] { return V23ActionModule::serviceAvailable("com.deepin.dde.GrandSearch"); },
        [] { callLater("com.deepin.dde.GrandSearch", "/com/deepin/dde/GrandSearch", "com.deepin.dde.GrandSearch", "SetVisible", {true}); },
        parent);

    modules << new V23ActionModule(QStringLiteral("screenshot"), V23ThemeModule::tr("Screenshot"),
        QStringLiteral("screenshot"),
        [] { return V23ActionModule::serviceAvailable("com.deepin.Screenshot"); },
        [] { callLater("com.deepin.Screenshot", "/com/deepin/Screenshot", "com.deepin.Screenshot", "StartScreenshot"); },
        parent);

    // deepin-screen-recorder starts or stops recording on stopRecord
    V23ActionModule *recorder = new V23ActionModule(QStringLiteral("screen-recorder"), V23ThemeModule::tr("Record"),
        QStringLiteral("screen-record"),
        [] { return V23ActionModule::serviceAvailable("com.deepin.ScreenRecorder"); },
        [] { callLater("com.deepin.ScreenRecorder", "/com/deepin/ScreenRecorder", "com.deepin.ScreenRecorder", "stopRecord"); },
        parent);
    V23DBusObject *recorderObject = new V23DBusObject(QDBusConnection::sessionBus(), "com.deepin.ScreenRecorder",
                                                      "/com/deepin/ScreenRecorder", "com.deepin.ScreenRecorder", recorder);
    QObject::connect(recorderObject, &V23DBusObject::propertiesChanged, recorder, [recorder, recorderObject] {
        const bool recording = recorderObject->property("IsRecording").toBool();
        recorder->setActive(recording, recording ? QStringLiteral("screen-recording") : QString(),
                            recording ? V23Quick::KeepColors : V23Quick::Mask);
    });
    modules << recorder;

    modules << new V23ActionModule(QStringLiteral("clipboard"), V23ThemeModule::tr("Clipboard"),
        QStringLiteral("clipboard"),
        [] { return V23ActionModule::serviceAvailable("org.deepin.dde.Clipboard1"); },
        [] { callLater("org.deepin.dde.Clipboard1", "/org/deepin/dde/Clipboard1", "org.deepin.dde.Clipboard1", "Toggle"); },
        parent);

    modules << new V23ActionModule(QStringLiteral("system-monitor"), V23ThemeModule::tr("System Monitor"),
        QStringLiteral("system-monitor"),
        [] { return QFile::exists("/usr/bin/deepin-system-monitor"); },
        [] {
            // the same way the built-in system monitor plugin opens it
            QProcess process;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("QT_QPA_PLATFORM", "dxcb;xcb");
            env.insert("QT_QPA_PLATFORMTHEME", "deepin");
            // a DTK6 app: the GXDE dtk2 style of the session crashes it
            env.remove("QT_STYLE_OVERRIDE");
            process.setProcessEnvironment(env);
            process.setProgram("deepin-system-monitor");
            process.startDetached();
        },
        parent);

    return modules;
}
