// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.

#ifndef V23DATETIMEPLUGIN_H
#define V23DATETIMEPLUGIN_H

#include "pluginsiteminterface.h"

#include <QPointer>

class QTimer;
class TipsWidget;
class V23CalendarWidget;
class V23DatetimeWidget;

// Deepin V23 replacement of the datetime plugin: the taskbar clock opens a
// calendar popup instead of the calendar app. It shares the plugin name, and
// so the settings, with the built-in datetime plugin.
class V23DatetimePlugin : public QObject, PluginsItemInterface, PluginsItemFactory<V23DatetimePlugin>
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterface PluginsItemFactoryInterface)
    Q_PLUGIN_METADATA(IID "com.deepin.dock.PluginsItemInterface" FILE "datetime.json")

public:
    explicit V23DatetimePlugin(QObject *parent = nullptr);

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;

    void pluginStateSwitched() override;
    bool pluginIsAllowDisable() override { return true; }
    bool pluginIsDisable() override;

    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;

    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;
    QWidget *itemPopupApplet(const QString &itemKey) override;

    const QString itemContextMenu(const QString &itemKey) override;
    void invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked) override;

    void positionChanged(const Dock::Position position) override;
    void pluginSettingsChanged() override;

private:
    void loadPlugin();
    void updateCurrentTimeString();
    void refreshPluginItemsVisible();
    void raiseCalendar();

private:
    QPointer<V23DatetimeWidget> m_centralWidget;
    QPointer<TipsWidget> m_dateTipsLabel;
    QPointer<V23CalendarWidget> m_calendarPopup;
    QTimer *m_refreshTimer;
};

#endif // V23DATETIMEPLUGIN_H
