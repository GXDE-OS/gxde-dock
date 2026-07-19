/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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
 * ----------------------------------------------------------------------------
 * The class for dock daemon entry object. This is a modified version of a
 * generated class from libdbusdframework, and the modification is that the
 * interface name is changed to top.gxde.daemon.dock.Entry. (The original name
 * is com.deepin.dde.daemon.Dock.Entry).
 *
 * I also re-formatted the file, everything else is kept as-is.
 */

#ifndef FRAME_DBUS_DOCKENTRYINTERFACE_H_
#define FRAME_DBUS_DOCKENTRYINTERFACE_H_

#include <DBusExtendedAbstractInterface>
#include <QtDBus/QtDBus>

#include <types/windowinfomap.h>

class DockEntryInterface : public DBusExtendedAbstractInterface {
    Q_OBJECT

public:
    explicit DockEntryInterface(const QString& service, const QString& path,
        const QDBusConnection& connection, QObject* parent = nullptr);
    ~DockEntryInterface() override;

    Q_PROPERTY(uint CurrentWindow
        READ currentWindow
        NOTIFY CurrentWindowChanged)

    uint currentWindow();

    Q_PROPERTY(QString DesktopFile READ desktopFile NOTIFY DesktopFileChanged)
    QString desktopFile();

    Q_PROPERTY(QString Icon READ icon NOTIFY IconChanged)
    QString icon();

    Q_PROPERTY(QString Id READ id NOTIFY IdChanged)
    QString id();

    Q_PROPERTY(bool IsActive READ isActive NOTIFY IsActiveChanged)
    bool isActive();

    Q_PROPERTY(bool IsDocked READ isDocked NOTIFY IsDockedChanged)
    bool isDocked();

    Q_PROPERTY(QString Menu READ menu NOTIFY MenuChanged)
    QString menu();

    Q_PROPERTY(QString Name READ name NOTIFY NameChanged)
    QString name();

    Q_PROPERTY(WindowInfoMap WindowInfos
        READ windowInfos
        NOTIFY WindowInfosChanged)

    WindowInfoMap windowInfos();

public Q_SLOTS:  // METHODS
    inline QDBusPendingReply<> Activate(uint timestamp) {
        return asyncCallWithArgumentList(QStringLiteral("Activate"),
                                         {QVariant::fromValue(timestamp)});
    }

    inline QDBusPendingReply<> Check() {
        return asyncCallWithArgumentList(QStringLiteral("Check"), {});
    }

    inline QDBusPendingReply<> ForceQuit() {
        return asyncCallWithArgumentList(QStringLiteral("ForceQuit"), {});
    }

    inline QDBusPendingReply<QList<uint>> GetAllowedCloseWindows() {
        return asyncCallWithArgumentList(
            QStringLiteral("GetAllowedCloseWindows"), {});
    }

    inline QDBusPendingReply<> HandleDragDrop(uint timestamp,
            const QStringList& files) {
        return asyncCallWithArgumentList(QStringLiteral("HandleDragDrop"),
            {QVariant::fromValue(timestamp),
             QVariant::fromValue(files)});
    }

    inline QDBusPendingReply<> HandleMenuItem(uint timestamp,
            const QString& id) {
        return asyncCallWithArgumentList(QStringLiteral("HandleMenuItem"),
            {QVariant::fromValue(timestamp),
            QVariant::fromValue(id)});
    }

    inline QDBusPendingReply<> NewInstance(uint timestamp) {
        return asyncCallWithArgumentList(QStringLiteral("NewInstance"),
            {QVariant::fromValue(timestamp)});
    }

    inline QDBusPendingReply<> PresentWindows() {
        return asyncCallWithArgumentList(QStringLiteral("PresentWindows"), {});
    }

    inline QDBusPendingReply<> RequestDock() {
        return asyncCallWithArgumentList(QStringLiteral("RequestDock"), {});
    }

    inline QDBusPendingReply<> RequestUndock() {
        return asyncCallWithArgumentList(QStringLiteral("RequestUndock"), {});
    }

Q_SIGNALS:
    void CurrentWindowChanged(uint value) const;
    void DesktopFileChanged(const QString& value) const;
    void IconChanged(const QString& value) const;
    void IdChanged(const QString& value) const;
    void IsActiveChanged(bool value) const;
    void IsDockedChanged(bool value) const;
    void MenuChanged(const QString& value) const;
    void NameChanged(const QString& value) const;
    void WindowInfosChanged(const WindowInfoMap& value) const;

private Q_SLOTS:
    void onPropertyChanged(const QString& propName, const QVariant& value);

private:
    uint m_currentWindow;
    QString m_desktopFile;
    QString m_icon;
    QString m_id;
    bool m_isActive;
    bool m_isDocked;
    QString m_menu;
    QString m_name;
    WindowInfoMap m_windowInfos;
};

#endif  // FRAME_DBUS_DOCKENTRYINTERFACE_H_
