// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23QUICKSETTINGSPLUGIN_H
#define V23QUICKSETTINGSPLUGIN_H

#include "pluginsiteminterface.h"

#include <QHash>
#include <QPointer>

class TipsWidget;
class V23DetailPage;
class V23DockIcon;
class V23QuickModule;
class V23QuickPanel;

// Deepin V23 quick settings: an icon opening the quick panel, which gathers
// the network, Bluetooth, sound, brightness, media and virtual keyboard, and
// the icons the user keeps on the dock. In V23 mode it takes over the
// built-in network, sound, power, notify and onboard plugins, like dde-dock
// 6.0 did.
class V23QuickSettingsPlugin : public QObject, PluginsItemInterface, PluginsItemFactory<V23QuickSettingsPlugin>
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterface PluginsItemFactoryInterface)
    Q_PLUGIN_METADATA(IID "com.deepin.dock.PluginsItemInterface" FILE "quick-settings.json")

public:
    explicit V23QuickSettingsPlugin(QObject *parent = nullptr);

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;

    bool pluginIsAllowDisable() override { return false; }
    bool pluginIsDisable() override { return false; }

    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;

    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;
    QWidget *itemPopupApplet(const QString &itemKey) override;
    const QString itemCommand(const QString &itemKey) override;

    const QString itemContextMenu(const QString &itemKey) override;
    void invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked) override;

    void refreshIcon(const QString &itemKey) override;
    void pluginSettingsChanged() override;

    Q_INVOKABLE QVariantList dockableItems();
    Q_INVOKABLE void setItemDocked(const QString &key, bool docked);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    V23QuickModule *module(const QString &key) const;
    QStringList dockedKeys();
    void setDocked(const QString &key, bool docked);
    void refreshDockedItems();
    void updateDockIcon(V23QuickModule *module);
    void updatePanelIcon();
    void hidePopups();

private:
    QList<V23QuickModule *> m_modules;
    V23QuickModule *m_sound = nullptr;
    QPointer<V23DockIcon> m_panelIcon;
    QPointer<V23QuickPanel> m_panel;
    QPointer<TipsWidget> m_tips;
    QString m_tipsKey;
    QHash<QString, QPointer<V23DockIcon>> m_dockIcons;
    QHash<QString, QPointer<V23DetailPage>> m_popupPages;
    QStringList m_shownKeys;
};

#endif // V23QUICKSETTINGSPLUGIN_H
