// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23quicksettingsplugin.h"
#include "v23basicmodules.h"
#include "v23bluetoothmodule.h"
#include "v23mediamodule.h"
#include "v23networkmodule.h"
#include "v23quickpanel.h"
#include "v23soundmodule.h"
#include "v23toolmodules.h"
#include "../../../widgets/tipswidget.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QJsonDocument>

#define QUICK_SETTINGS_KEY "quick-settings"
#define DOCKED_KEY "docked"

namespace {
// what the tray of Deepin V23 shows besides the quick panel button
const QStringList DefaultDocked { "network", "power" };

// the order of the icons, left to right, ending with the quick panel button
const QStringList DefaultOrder { "onboard", "bluetooth", "notification", "network", "power", QUICK_SETTINGS_KEY };
}

V23QuickSettingsPlugin::V23QuickSettingsPlugin(QObject *parent)
    : QObject(parent)
{
}

const QString V23QuickSettingsPlugin::pluginName() const
{
    return QStringLiteral(QUICK_SETTINGS_KEY);
}

const QString V23QuickSettingsPlugin::pluginDisplayName() const
{
    return tr("Quick Settings");
}

void V23QuickSettingsPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;
    if (m_panel)
        return;

    // the tiles show in the order of the quickPlugins of dde-shell
    m_sound = new V23SoundModule(this);
    m_modules << new V23NetworkModule(this)
              << new V23BluetoothModule(this)
              << new V23ThemeModule(this)
              << new V23OnboardModule(this)
              << createActionModules(this)
              << new V23MediaModule(this)
              << new V23BrightnessModule(this)
              << m_sound
              << new V23PowerModule(this)
              << new V23NotificationModule(this);

    m_tips = new TipsWidget;
    m_tips->setVisible(false);

    // the quick panel button of dde-shell: the volume and the panel icon
    m_panelIcon = new V23DockIcon;
    connect(m_panelIcon, &V23DockIcon::wheelScrolled, m_sound, &V23QuickModule::dockWheel);
    updatePanelIcon();
    m_panel = new V23QuickPanel(m_modules);
    connect(m_panel, &V23QuickPanel::requestHide, this, &V23QuickSettingsPlugin::hidePopups);

    for (V23QuickModule *module : m_modules) {
        connect(module, &V23QuickModule::changed, this, [this, module] {
            updateDockIcon(module);
            if (module == m_sound)
                updatePanelIcon();
            if (m_tipsKey == module->key() && m_tips)
                m_tips->setText(module->tipText());
        });
        connect(module, &V23QuickModule::availableChanged, this, &V23QuickSettingsPlugin::refreshDockedItems);
        connect(module, &V23QuickModule::requestHidePopup, this, &V23QuickSettingsPlugin::hidePopups);
    }

    // the V23 light theme can be turned on and off at any time
    qApp->installEventFilter(this);

    m_proxyInter->itemAdded(this, QUICK_SETTINGS_KEY);
    refreshDockedItems();
}

V23QuickModule *V23QuickSettingsPlugin::module(const QString &key) const
{
    for (V23QuickModule *module : m_modules) {
        if (module->key() == key)
            return module;
    }

    return nullptr;
}

QStringList V23QuickSettingsPlugin::dockedKeys()
{
    return m_proxyInter->getValue(this, DOCKED_KEY, DefaultDocked).toStringList();
}

void V23QuickSettingsPlugin::setDocked(const QString &key, bool docked)
{
    QStringList keys = dockedKeys();
    keys.removeAll(key);
    if (docked)
        keys << key;
    m_proxyInter->saveValue(this, DOCKED_KEY, keys);
    refreshDockedItems();
}

void V23QuickSettingsPlugin::refreshDockedItems()
{
    const QStringList docked = dockedKeys();
    for (V23QuickModule *module : m_modules) {
        const QString key = module->key();
        const bool show = module->isDockable() && module->isAvailable() && docked.contains(key);
        if (show == m_shownKeys.contains(key))
            continue;

        if (show) {
            if (!m_dockIcons.value(key)) {
                V23DockIcon *icon = new V23DockIcon;
                connect(icon, &V23DockIcon::wheelScrolled, module, &V23QuickModule::dockWheel);
                m_dockIcons.insert(key, icon);
            }
            updateDockIcon(module);
            m_shownKeys << key;
            m_proxyInter->itemAdded(this, key);
        } else {
            m_shownKeys.removeAll(key);
            m_proxyInter->itemRemoved(this, key);
        }
    }
}

void V23QuickSettingsPlugin::updateDockIcon(V23QuickModule *module)
{
    if (V23DockIcon *icon = m_dockIcons.value(module->key()))
        icon->setIcon(module->dockIcon());
}

void V23QuickSettingsPlugin::updatePanelIcon()
{
    if (m_panelIcon)
        m_panelIcon->setIcons({ m_sound->dockIcon(), V23Icon { QStringLiteral("dock-control-panel") } });
}

void V23QuickSettingsPlugin::hidePopups()
{
    m_proxyInter->requestSetAppletVisible(this, QUICK_SETTINGS_KEY, false);
    for (const QString &key : m_shownKeys)
        m_proxyInter->requestSetAppletVisible(this, key, false);
}

int V23QuickSettingsPlugin::itemSortKey(const QString &itemKey)
{
    // after the other plugins of the same default place
    const int fallback = 10 + DefaultOrder.indexOf(itemKey);
    return m_proxyInter->getValue(this, QString("pos_%1").arg(itemKey), fallback).toInt();
}

void V23QuickSettingsPlugin::setSortKey(const QString &itemKey, const int order)
{
    m_proxyInter->saveValue(this, QString("pos_%1").arg(itemKey), order);
}

QWidget *V23QuickSettingsPlugin::itemWidget(const QString &itemKey)
{
    if (itemKey == QUICK_SETTINGS_KEY)
        return m_panelIcon;

    return m_dockIcons.value(itemKey);
}

QWidget *V23QuickSettingsPlugin::itemTipsWidget(const QString &itemKey)
{
    // reset the text each time, the font size may have changed meanwhile
    m_tipsKey = itemKey;
    if (itemKey == QUICK_SETTINGS_KEY) {
        m_tips->setText(tr("Quick actions"));
    } else if (V23QuickModule *module = this->module(itemKey)) {
        module->aboutToShowTip();
        m_tips->setText(module->tipText());
    }

    return m_tips;
}

QWidget *V23QuickSettingsPlugin::itemPopupApplet(const QString &itemKey)
{
    if (itemKey == QUICK_SETTINGS_KEY)
        return m_panel;

    V23QuickModule *module = this->module(itemKey);
    if (!module || !module->hasPage())
        return nullptr;

    if (!m_popupPages.value(itemKey)) {
        // its own page, as the panel may hold the other one
        QWidget *content = module->createPage(nullptr);
        m_popupPages.insert(itemKey, new V23DetailPage(module->displayName(), content, false));
    }

    return m_popupPages.value(itemKey);
}

const QString V23QuickSettingsPlugin::itemCommand(const QString &itemKey)
{
    V23QuickModule *module = this->module(itemKey);
    if (!module || module->hasPage())
        return QString();

    return module->dockCommand();
}

const QString V23QuickSettingsPlugin::itemContextMenu(const QString &itemKey)
{
    QList<QVariant> items;
    auto addItem = [&items](const QString &id, const QString &text) {
        QMap<QString, QVariant> item;
        item["itemId"] = id;
        item["itemText"] = text;
        item["isActive"] = true;
        items << item;
    };

    if (itemKey == QUICK_SETTINGS_KEY) {
        // dde-dock lets the tiles be dragged onto the dock, here the menu does
        const QStringList docked = dockedKeys();
        for (V23QuickModule *module : m_modules) {
            if (!module->isDockable() || !module->isAvailable())
                continue;

            if (docked.contains(module->key()))
                addItem("undock:" + module->key(), tr("Remove %1 from Dock").arg(module->displayName()));
            else
                addItem("dock:" + module->key(), tr("Show %1 on Dock").arg(module->displayName()));
        }
    } else if (V23QuickModule *module = this->module(itemKey)) {
        for (const auto &item : module->menuItems())
            addItem(item.first, item.second);
        addItem("undock:" + itemKey, tr("Remove from Dock"));
    }

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void V23QuickSettingsPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(checked);

    if (menuId.startsWith("dock:")) {
        setDocked(menuId.mid(5), true);
    } else if (menuId.startsWith("undock:")) {
        setDocked(menuId.mid(7), false);
    } else if (V23QuickModule *module = this->module(itemKey)) {
        module->invokeMenuItem(menuId);
    }
}

void V23QuickSettingsPlugin::refreshIcon(const QString &itemKey)
{
    if (QWidget *widget = itemWidget(itemKey))
        widget->update();
}

void V23QuickSettingsPlugin::pluginSettingsChanged()
{
    // another dock may have changed what is docked
    refreshDockedItems();
}

bool V23QuickSettingsPlugin::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::DynamicPropertyChange
            && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == PROP_V23_LIGHT_THEME) {
        if (m_panelIcon)
            m_panelIcon->update();
        for (const QPointer<V23DockIcon> &icon : m_dockIcons) {
            if (icon)
                icon->update();
        }
    }

    return QObject::eventFilter(watched, event);
}
