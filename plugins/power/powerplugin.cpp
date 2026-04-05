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

#include "powerplugin.h"
#include "dbus/dbusaccount.h"

#include <QIcon>

#define PLUGIN_STATE_KEY    "enable"

PowerPlugin::PowerPlugin(QObject *parent)
    : QObject(parent),

      m_pluginLoaded(false),
      m_tipsLabel(new TipsWidget)
{
    m_tipsLabel->setVisible(false);
    m_tipsLabel->setObjectName("power");
}

const QString PowerPlugin::pluginName() const
{
    return "power";
}

const QString PowerPlugin::pluginDisplayName() const
{
    return tr("Power");
}

QWidget *PowerPlugin::itemWidget(const QString &itemKey)
{
    if (itemKey == POWER_KEY)
        return m_powerStatusWidget;

    return nullptr;
}

QWidget *PowerPlugin::itemTipsWidget(const QString &itemKey)
{
    const BatteryPercentageMap data = m_powerInter->batteryPercentage();

    if (data.isEmpty()) {
        return nullptr;
    }

    m_tipsLabel->setObjectName(itemKey);

    refreshTipsData();

    return m_tipsLabel;
}

void PowerPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    if (!pluginIsDisable()) {
        loadPlugin();
    }
}

void PowerPlugin::pluginStateSwitched()
{
    m_proxyInter->saveValue(this, PLUGIN_STATE_KEY, pluginIsDisable());

    refreshPluginItemsVisible();
}

bool PowerPlugin::pluginIsDisable()
{
    return !m_proxyInter->getValue(this, PLUGIN_STATE_KEY, true).toBool();
}

const QString PowerPlugin::itemCommand(const QString &itemKey)
{
    if (itemKey == POWER_KEY)
        return QString("dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.ShowModule \"string:power\"");

    return QString();
}

const QString PowerPlugin::itemContextMenu(const QString &itemKey)
{
    if (itemKey != POWER_KEY) {
        return QString();
    }

    QList<QVariant> items;
    items.reserve(6);

    QMap<QString, QVariant> power;
    power["itemId"] = "power";
    power["itemText"] = tr("Power settings");
    power["isActive"] = true;
    items.push_back(power);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;

    return QJsonDocument::fromVariant(menu).toJson();
}

void PowerPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(itemKey)
    Q_UNUSED(checked)

    if (menuId == "power")
        QProcess::startDetached("dbus-send --print-reply --dest=com.deepin.dde.ControlCenter /com/deepin/dde/ControlCenter com.deepin.dde.ControlCenter.ShowModule \"string:power\"");
}

void PowerPlugin::refreshIcon(const QString &itemKey)
{
    if (itemKey == POWER_KEY) {
        m_powerStatusWidget->refreshIcon();
    }
}

int PowerPlugin::itemSortKey(const QString &itemKey)
{
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(displayMode());

    return m_proxyInter->getValue(this, key, displayMode() == Dock::DisplayMode::Fashion ? 3 : 3).toInt();
}

void PowerPlugin::setSortKey(const QString &itemKey, const int order)
{
    const QString key = QString("pos_%1_%2").arg(itemKey).arg(displayMode());

    m_proxyInter->saveValue(this, key, order);
}

void PowerPlugin::pluginSettingsChanged()
{
    refreshPluginItemsVisible();
}

void PowerPlugin::updateBatteryVisible()
{
    const bool exist = !m_powerInter->batteryPercentage().isEmpty();

    if (!exist)
        m_proxyInter->itemRemoved(this, POWER_KEY);
    else if (exist && !pluginIsDisable())
        m_proxyInter->itemAdded(this, POWER_KEY);
}

void PowerPlugin::loadPlugin()
{
    if (m_pluginLoaded) {
        qDebug() << "power plugin has been loaded! return";
        return;
    }

    m_pluginLoaded = true;

    m_powerStatusWidget = new PowerStatusWidget;
    m_powerInter = new DBusPower(this);

    m_systemPowerInter = new SystemPowerInter("com.deepin.system.Power", "/com/deepin/system/Power", QDBusConnection::systemBus(), this);
    m_systemPowerInter->setSync(true);

    connect(m_systemPowerInter, &SystemPowerInter::BatteryStatusChanged, this, &PowerPlugin::refreshTipsData);
    connect(m_systemPowerInter, &SystemPowerInter::BatteryTimeToEmptyChanged, this, &PowerPlugin::refreshTipsData);
    connect(m_systemPowerInter, &SystemPowerInter::BatteryTimeToFullChanged, this, &PowerPlugin::refreshTipsData);

    connect(m_powerInter, &DBusPower::BatteryPercentageChanged, this, &PowerPlugin::updateBatteryVisible);

    updateBatteryVisible();
}

void PowerPlugin::refreshPluginItemsVisible()
{
    if (pluginIsDisable()) {
        m_proxyInter->itemRemoved(this, POWER_KEY);
    } else {
        if (!m_pluginLoaded) {
            loadPlugin();
            return;
        }
        updateBatteryVisible();
    }
}

int PowerPlugin::getBatteryHealth()
{
    // 尝试使用upower命令获取 - 优先使用battery_BAT0
    QProcess process;
    process.start("upower", QStringList() << "-i" << "/org/freedesktop/UPower/devices/battery_BAT0");
    process.waitForFinished(1000);
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        
        // 直接搜索capacity字符串
        int capacityIndex = output.indexOf("capacity:");
        if (capacityIndex != -1) {
            QString capacityLine = output.mid(capacityIndex).split("\n").first();
            QRegularExpression rx("(\\d+)%");
            QRegularExpressionMatch match = rx.match(capacityLine);
            if (match.hasMatch()) {
                return match.captured(1).toInt();
            }
        }
    }
    
    // 尝试从系统文件获取电池健康度
    QDir batteryDir("/sys/class/power_supply");
    QStringList batteryNames = batteryDir.entryList(QStringList() << "BAT*");
    
    if (!batteryNames.isEmpty()) {
        QString batteryPath = batteryDir.absolutePath() + "/" + batteryNames.first();
        
        // 读取设计容量和当前满电容量
        QFile designFile(batteryPath + "/charge_full_design");
        QFile fullFile(batteryPath + "/charge_full");
        
        if (designFile.open(QIODevice::ReadOnly) && fullFile.open(QIODevice::ReadOnly)) {
            bool ok1, ok2;
            int designCapacity = designFile.readAll().trimmed().toInt(&ok1);
            int fullCapacity = fullFile.readAll().trimmed().toInt(&ok2);
            
            if (ok1 && ok2 && designCapacity > 0) {
                int health = static_cast<int>((static_cast<double>(fullCapacity) / designCapacity) * 100);
                return qMin(100, qMax(0, health));
            }
        }
        
        // 尝试读取能量容量
        QFile energyFullFile(batteryPath + "/energy_full");
        QFile energyFullDesignFile(batteryPath + "/energy_full_design");
        
        if (energyFullFile.open(QIODevice::ReadOnly) && energyFullDesignFile.open(QIODevice::ReadOnly)) {
            bool ok1, ok2;
            double energyFull = energyFullFile.readAll().trimmed().toDouble(&ok1);
            double energyFullDesign = energyFullDesignFile.readAll().trimmed().toDouble(&ok2);
            
            if (ok1 && ok2 && energyFullDesign > 0) {
                int health = static_cast<int>((energyFull / energyFullDesign) * 100);
                return qMin(100, qMax(0, health));
            }
        }
    }
    
    // 尝试使用DisplayDevice
    process.start("upower", QStringList() << "-i" << "/org/freedesktop/UPower/devices/DisplayDevice");
    process.waitForFinished(1000);
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        
        // 直接搜索capacity字符串
        int capacityIndex = output.indexOf("capacity:");
        if (capacityIndex != -1) {
            QString capacityLine = output.mid(capacityIndex).split("\n").first();
            QRegularExpression rx("(\\d+)%");
            QRegularExpressionMatch match = rx.match(capacityLine);
            if (match.hasMatch()) {
                return match.captured(1).toInt();
            }
        }
    }
    
    // 如果无法获取，返回默认值
    return 0;
}

void PowerPlugin::refreshTipsData()
{
    const BatteryPercentageMap data = m_powerInter->batteryPercentage();

    const uint percentage = qMin(100.0, qMax(0.0, data.value("Display")));
    const QString value = QString("%1%").arg(std::round(percentage));
    const int batteryState = m_powerInter->batteryState()["Display"];

    // 获取电池健康度
    int healthPercentage = getBatteryHealth();

    if (m_powerInter->onBattery()) {
        qulonglong timeToEmpty = m_systemPowerInter->batteryTimeToEmpty();
        QDateTime time = QDateTime::fromTime_t(timeToEmpty).toUTC();
        uint hour = time.toString("hh").toUInt();
        uint min = time.toString("mm").toUInt();

        QString tips;

        if (hour == 0) {
            tips = tr("Capacity %1, %2 min remaining, Health: %3%").arg(value).arg(min).arg(healthPercentage);
        }
        else {
            tips = tr("Capacity %1, %2 hr %3 min remaining, Health: %4%").arg(value).arg(hour).arg(min).arg(healthPercentage);
        }

        m_tipsLabel->setText(tips);
    }
    else {
        if (batteryState == BatteryState::FULLY_CHARGED || percentage == 100.) {
            m_tipsLabel->setText(tr("Charged %1, Health: %2%").arg(value).arg(healthPercentage));
        }
        else {
            qulonglong timeToFull = m_systemPowerInter->batteryTimeToFull();
            QDateTime time = QDateTime::fromTime_t(timeToFull).toUTC();
            uint hour = time.toString("hh").toUInt();
            uint min = time.toString("mm").toUInt();

            QString tips;

            if (hour == 0) {
                tips = tr("Charging %1, %2 min until full, health: %3%").arg(value).arg(min).arg(healthPercentage);
            }
            else {
                tips = tr("Charging %1, %2 hr %3 min until full, health: %4%").arg(value).arg(hour).arg(min).arg(healthPercentage);
            }

            m_tipsLabel->setText(tips);
        }
    }
}
