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

#include <QHBoxLayout>

#include <QWidget>

#define PLUGIN_STATE_KEY    "disabled"

// tooltip 中最多展示的占用进程数量，避免列表过长
#define MAX_TIPS_PROCESS_COUNT 5

PermissionMonitorPlugin::PermissionMonitorPlugin(QObject *parent)
    : QObject(parent)
    , m_container(new QWidget)
    , m_cameraItem(new MonitorItem({QStringLiteral("camera-web"),
                                    QStringLiteral("camera-video"),
                                    QStringLiteral("camera-photo")},
                                   QStringLiteral(":/icons/resources/camera.svg")))
    , m_microphoneItem(new MonitorItem({QStringLiteral("audio-input-microphone")},
                                       QStringLiteral(":/icons/resources/microphone.svg")))
    , m_tipsWidget(new TipsWidget)
    , m_itemAdded(false)
    , m_deviceMonitor(new DeviceMonitor(this))
{
    // 摄像头与麦克风图标并排放在同一 item 内，作为一个整体显示；
    // 四周留出边距，与相邻插件保持宽松间距（其它插件 item 自带留白，
    // 本容器内容较满，需要自行补足）
    QHBoxLayout *layout = new QHBoxLayout(m_container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(m_cameraItem);
    layout->addWidget(m_microphoneItem);

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
    Q_UNUSED(itemKey)

    return m_container;
}

QWidget *PermissionMonitorPlugin::itemTipsWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey)

    m_tipsWidget->setText(tipsText());

    return m_tipsWidget;
}

bool PermissionMonitorPlugin::pluginIsDisable()
{
    // dock 加载 .so 时会创建一个从不调用 init() 的根实例（仅作为工厂接口
    // 使用），该实例的 m_proxyInter 为空，不能与 dock 交互
    if (!m_proxyInter) {
        return false;
    }

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

    // 默认放在插件区末段（7），避开其它插件常用的 1~5
    return m_proxyInter->getValue(this, key, 7).toInt();
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
    Q_UNUSED(itemKey)

    m_cameraItem->refreshIcon();
    m_microphoneItem->refreshIcon();
}

QString PermissionMonitorPlugin::tipsText() const
{
    QStringList texts;

    if (m_deviceMonitor->deviceInUse(DeviceMonitor::Camera)) {
        texts << tipsTextFor(DeviceMonitor::Camera);
    }
    if (m_deviceMonitor->deviceInUse(DeviceMonitor::Microphone)) {
        texts << tipsTextFor(DeviceMonitor::Microphone);
    }

    return texts.join(QStringLiteral("; "));
}

QString PermissionMonitorPlugin::tipsTextFor(DeviceMonitor::DeviceType type) const
{
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
    // 根实例（未调用 init）不与 dock 交互，事件只更新其内部状态
    if (!m_proxyInter) {
        return;
    }

    const bool disabled = pluginIsDisable();
    const bool cameraInUse = m_deviceMonitor->deviceInUse(DeviceMonitor::Camera);
    const bool microphoneInUse = m_deviceMonitor->deviceInUse(DeviceMonitor::Microphone);

    // 任一设备被占用时整个 item 显示，图标由容器内子 widget 按各自状态显隐
    m_cameraItem->setVisible(!disabled && cameraInUse);
    m_microphoneItem->setVisible(!disabled && microphoneInUse);
    updateItemVisible(!disabled && (cameraInUse || microphoneInUse));
}

void PermissionMonitorPlugin::updateItemVisible(bool visible)
{
    if (m_itemAdded == visible) {
        return;
    }

    m_itemAdded = visible;

    if (visible) {
        m_proxyInter->itemAdded(this, pluginName());
    } else {
        m_proxyInter->itemRemoved(this, pluginName());
    }
}

void PermissionMonitorPlugin::refreshTips()
{
    if (m_tipsWidget->isVisible()) {
        m_tipsWidget->setText(tipsText());
    }
}
