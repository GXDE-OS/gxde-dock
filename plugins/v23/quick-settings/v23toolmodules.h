// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23TOOLMODULES_H
#define V23TOOLMODULES_H

#include "v23quickmodule.h"

#include <QPointer>
#include <functional>

class V23DBusObject;
class V23ListView;

// The light or dark theme, a tile like the eye comfort plugin of Deepin V23
// shows without color temperature support
class V23ThemeModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23ThemeModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("theme"); }
    QString displayName() const override;
    bool isAvailable() const override;
    bool isDockable() const override { return false; }

    QWidget *createTile(QWidget *parent) override;
    bool hasPage() const override { return true; }
    QWidget *createPage(QWidget *parent) override;

private:
    bool isDark() const;
    void update();

private:
    V23DBusObject *m_appearance;
    QPointer<V23QuickTile> m_tile;
    QList<QPointer<V23ListView>> m_pages;
    bool m_available;
};

// A single tile that runs something and closes the panel: the grand search,
// the screenshot, the screen recorder, the clipboard, the system monitor
class V23ActionModule : public V23QuickModule
{
    Q_OBJECT

public:
    V23ActionModule(const QString &key, const QString &displayName, const QString &icon,
                    std::function<bool()> available, std::function<void()> action, QObject *parent = nullptr);

    QString key() const override { return m_key; }
    QString displayName() const override { return m_displayName; }
    bool isAvailable() const override { return m_available(); }
    bool isDockable() const override { return false; }

    QWidget *createTile(QWidget *parent) override;

    // the recorder shows that it records
    void setActive(bool active, const QString &icon = QString(), V23Quick::IconMode mode = V23Quick::Mask);

    // true when a D-Bus service runs, or can be started
    static bool serviceAvailable(const QString &service);

private:
    QString m_key;
    QString m_displayName;
    QString m_icon;
    std::function<bool()> m_available;
    std::function<void()> m_action;
    QPointer<V23QuickTile> m_tile;
};

// the action modules of the quick panel, in their order
QList<V23QuickModule *> createActionModules(QObject *parent);

#endif // V23TOOLMODULES_H
