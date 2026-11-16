/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#include "permissionmonitorplugin.h"

#include "../widgets/tipswidget.h"

#define PLUGIN_STATE_KEY    "disabled"

// tooltip 中最多展示的占用进程数量，避免列表过长
#define MAX_TIPS_PROCESS_COUNT 5

namespace {

QString cameraKey()
{
    return QStringLiteral("camera");
}

QString microphoneKey()
{
    return QStringLiteral("microphone");
}

} // namespace

PermissionMonitorPlugin::PermissionMonitorPlugin(QObject *parent)
    : QObject(parent)
    , m_cameraItem(new MonitorItem({QStringLiteral("camera-web"),
                                    QStringLiteral("camera-video"),
                                    QStringLiteral("camera-photo")},
                                   QStringLiteral(":/icons/resources/camera.svg")))
    , m_microphoneItem(new MonitorItem({QStringLiteral("audio-input-microphone")},
                                       QStringLiteral(":/icons/resources/microphone.svg")))
    , m_tipsWidget(new TipsWidget)
    , m_deviceMonitor(new DeviceMonitor(this))
{
    m_cameraItem->setVisible(false);
    m_microphoneItem->setVisible(false);

    connect(m_deviceMonitor, &DeviceMonitor::usageChanged, this, &PermissionMonitorPlugin::onUsageChanged);
}

const QString PermissionMonitorPlugin::pluginName() const
{
    return QStringLiteral("permission-monitor");
}

const QString PermissionMonitorPlugin::pluginDisplayName() const
{
    return tr("Permission Monitor");
}

void PermissionMonitorPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    m_deviceMonitor->start();
    updateAllItems();
}

QWidget *PermissionMonitorPlugin::itemWidget(const QString &itemKey)
{
    return typeForKey(itemKey) == DeviceMonitor::Camera ? m_cameraItem : m_microphoneItem;
}

QWidget *PermissionMonitorPlugin::itemTipsWidget(const QString &itemKey)
{
    m_currentTipsKey = itemKey;
    m_tipsWidget->setText(tipsTextFor(itemKey));

    return m_tipsWidget;
}

bool PermissionMonitorPlugin::pluginIsDisable()
{
    return m_proxyInter->getValue(this, PLUGIN_STATE_KEY, false).toBool();
}

void PermissionMonitorPlugin::pluginStateSwitched()
{
    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, !pluginIsDisable());

    updateAllItems();
}

int PermissionMonitorPlugin::itemSortKey(const QString &itemKey)
{
    const QString key = QStringLiteral("pos_%1_%2").arg(itemKey).arg(displayMode());
    const bool camera = typeForKey(itemKey) == DeviceMonitor::Camera;
    const int defaultValue = displayMode() == Dock::Fashion ? (camera ? 2 : 3)
                                                            : (camera ? 5 : 6);

    return m_proxyInter->getValue(this, key, defaultValue).toInt();
}

void PermissionMonitorPlugin::setSortKey(const QString &itemKey, const int order)
{
    const QString key = QStringLiteral("pos_%1_%2").arg(itemKey).arg(displayMode());
    m_proxyInter->saveValue(this, key, order);
}

void PermissionMonitorPlugin::displayModeChanged(const Dock::DisplayMode displayMode)
{
    m_cameraItem->setDockDisplayMode(displayMode);
    m_microphoneItem->setDockDisplayMode(displayMode);
}

void PermissionMonitorPlugin::refreshIcon(const QString &itemKey)
{
    static_cast<MonitorItem *>(itemWidget(itemKey))->refreshIcon();
}

DeviceMonitor::DeviceType PermissionMonitorPlugin::typeForKey(const QString &itemKey) const
{
    return itemKey == cameraKey() ? DeviceMonitor::Camera : DeviceMonitor::Microphone;
}

QString PermissionMonitorPlugin::tipsTextFor(const QString &itemKey) const
{
    DeviceMonitor::DeviceType type = typeForKey(itemKey);
    const QStringList processes = m_deviceMonitor->usingProcesses(type);

    if (processes.isEmpty()) {
        return type == DeviceMonitor::Camera ? tr("Camera is being used")
                                             : tr("Microphone is being used");
    }

    const QStringList names = processes.mid(0, MAX_TIPS_PROCESS_COUNT);
    const QString processText = names.join(QStringLiteral(", "));

    return type == DeviceMonitor::Camera ? tr("Camera is being used by %1").arg(processText)
                                         : tr("Microphone is being used by %1").arg(processText);
}

void PermissionMonitorPlugin::onUsageChanged()
{
    updateAllItems();
    refreshTips();
}

void PermissionMonitorPlugin::updateAllItems()
{
    const bool disabled = pluginIsDisable();

    updateItemVisible(cameraKey(), !disabled && m_deviceMonitor->deviceInUse(DeviceMonitor::Camera));
    updateItemVisible(microphoneKey(), !disabled && m_deviceMonitor->deviceInUse(DeviceMonitor::Microphone));
}

void PermissionMonitorPlugin::updateItemVisible(const QString &itemKey, bool visible)
{
    if (m_itemAdded.value(itemKey, false) == visible) {
        return;
    }

    m_itemAdded[itemKey] = visible;

    if (visible) {
        m_proxyInter->itemAdded(this, itemKey);
    } else {
        m_proxyInter->itemRemoved(this, itemKey);
    }
}

void PermissionMonitorPlugin::refreshTips()
{
    if (m_tipsWidget->isVisible() && !m_currentTipsKey.isEmpty()) {
        m_tipsWidget->setText(tipsTextFor(m_currentTipsKey));
    }
}
