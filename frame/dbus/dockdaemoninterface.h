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
 * The class for dock daemon manager object.This is a modified version of a
 * generated class from libdbusdframework, and the modification is that the
 * interface name is changed to top.gxde.daemon.dock. (The original name
 * is com.deepin.dde.daemon.Dock).
 */

#ifndef FRAME_DBUS_DOCKDAEMONINTERFACE_H_
#define FRAME_DBUS_DOCKDAEMONINTERFACE_H_

#include <DBusExtendedAbstractInterface>
#include <QtDBus/QtDBus>

class DockDaemonInterface : public DBusExtendedAbstractInterface {
    Q_OBJECT

public:
    explicit DockDaemonInterface(const QString& service, const QString& path,
        const QDBusConnection& connection, QObject* parent = nullptr);
    ~DockDaemonInterface() override;

    Q_PROPERTY(int DisplayMode
        READ displayMode
        WRITE setDisplayMode
        NOTIFY DisplayModeChanged)

    int displayMode();
    void setDisplayMode(int value);

    Q_PROPERTY(QList<QDBusObjectPath> Entries
        READ entries
        NOTIFY EntriesChanged)

    QList<QDBusObjectPath> entries();

    Q_PROPERTY(int HideMode
        READ hideMode
        WRITE setHideMode
        NOTIFY HideModeChanged)

    int hideMode();
    void setHideMode(int value);

    Q_PROPERTY(int HideState
        READ hideState
        NOTIFY HideStateChanged)

    int hideState();

    Q_PROPERTY(uint IconSize
        READ iconSize
        WRITE setIconSize
        NOTIFY IconSizeChanged)

    uint iconSize();
    void setIconSize(uint value);

    Q_PROPERTY(double Opacity READ opacity NOTIFY OpacityChanged)
    double opacity();

    Q_PROPERTY(int Position
        READ position
        WRITE setPosition
        NOTIFY PositionChanged)

    int position();
    void setPosition(int value);

    Q_PROPERTY(uint ShowTimeout
        READ showTimeout
        WRITE setShowTimeout
        NOTIFY ShowTimeoutChanged)

    uint showTimeout();
    void setShowTimeout(uint value);

    Q_PROPERTY(bool WindowSplit
        READ windowSplit
        WRITE setWindowSplit
        NOTIFY WindowSplitChanged)

    bool windowSplit();
    void setWindowSplit(bool value);

public Q_SLOTS:  // METHODS
    inline QDBusPendingReply<> ActivateWindow(uint win) {
        return asyncCallWithArgumentList(QStringLiteral("ActivateWindow"),
            {QVariant::fromValue(win)});
    }

    inline QDBusPendingReply<> CancelPreviewWindow() {
        return asyncCallWithArgumentList(QStringLiteral("CancelPreviewWindow"),
            {});
    }

    inline QDBusPendingReply<> CloseWindow(uint win) {
        return asyncCallWithArgumentList(QStringLiteral("CloseWindow"),
            {QVariant::fromValue(win)});
    }

    inline QDBusPendingReply<QString> GetPluginSettings() {
        return asyncCallWithArgumentList(QStringLiteral("GetPluginSettings"),
            {});
    }

    inline QDBusPendingReply<bool> IsOnDock(const QString& desktopFile) {
        return asyncCallWithArgumentList(QStringLiteral("IsOnDock"),
            {QVariant::fromValue(desktopFile)});
    }

    inline QDBusPendingReply<> MergePluginSettings(const QString& jsonStr) {
        return asyncCallWithArgumentList(QStringLiteral("MergePluginSettings"),
            {QVariant::fromValue(jsonStr)});
    }

    inline QDBusPendingReply<> MoveEntry(int index, int newIndex) {
        return asyncCallWithArgumentList(QStringLiteral("MoveEntry"),
            {QVariant::fromValue(index),
            QVariant::fromValue(newIndex)});
    }

    inline QDBusPendingReply<> PreviewWindow(uint win) {
        return asyncCallWithArgumentList(QStringLiteral("PreviewWindow"),
            {QVariant::fromValue(win)});
    }

    inline QDBusPendingReply<> RemovePluginSettings(const QString& key1,
            const QStringList& key2List) {
        return asyncCallWithArgumentList(QStringLiteral("RemovePluginSettings"),
            {QVariant::fromValue(key1),
            QVariant::fromValue(key2List)});
    }

    inline QDBusPendingReply<bool> RequestDock(const QString& desktopFile,
            int index) {
        return asyncCallWithArgumentList(QStringLiteral("RequestDock"),
            {QVariant::fromValue(desktopFile),
            QVariant::fromValue(index)});
    }

    inline QDBusPendingReply<bool> RequestUndock(const QString& desktopFile) {
        return asyncCallWithArgumentList(QStringLiteral("RequestUndock"),
            {QVariant::fromValue(desktopFile)});
    }

    inline QDBusPendingReply<> SetFrontendWindowRect(int x, int y, uint width,
            uint height) {
        return asyncCallWithArgumentList(QStringLiteral(
                "SetFrontendWindowRect"),
            {QVariant::fromValue(x),
            QVariant::fromValue(y),
            QVariant::fromValue(width),
            QVariant::fromValue(height)});
    }

    inline QDBusPendingReply<> SetPluginSettings(const QString& jsonStr) {
        return asyncCallWithArgumentList(QStringLiteral("SetPluginSettings"),
            {QVariant::fromValue(jsonStr)});
    }

Q_SIGNALS:  // SIGNALS
    void EntryAdded(const QDBusObjectPath& path, int index);
    void EntryRemoved(const QString& entryId);
    void PluginSettingsSynced();
    void ServiceRestarted();

    // properties
    void DisplayModeChanged(int value) const;
    void EntriesChanged(const QList<QDBusObjectPath>& value) const;
    void HideModeChanged(int value) const;
    void HideStateChanged(int value) const;
    void IconSizeChanged(uint value) const;
    void OpacityChanged(double value) const;
    void PositionChanged(int value) const;
    void ShowTimeoutChanged(uint value) const;
    void WindowSplitChanged(bool value) const;

private Q_SLOTS:
    void onPropertyChanged(const QString& propName, const QVariant& value);

private:
    int m_displayMode;
    QList<QDBusObjectPath> m_entries;
    int m_hideMode;
    int m_hideState;
    uint m_iconSize;
    double m_opacity;
    int m_position;
    uint m_showTimeout;
    bool m_windowSplit;
};

#endif  // FRAME_DBUS_DOCKDAEMONINTERFACE_H_
