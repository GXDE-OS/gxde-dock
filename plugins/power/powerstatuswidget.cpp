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

#include "powerstatuswidget.h"
#include "powerplugin.h"

#include <QPainter>
#include <QIcon>
#include <QMouseEvent>

PowerStatusWidget::PowerStatusWidget(QWidget *parent)
    : QWidget(parent),

      m_powerInter(new DBusPower(this))
{
//    QIcon::setThemeName("deepin");

    connect(m_powerInter, &DBusPower::BatteryPercentageChanged, this, static_cast<void (PowerStatusWidget::*)()>(&PowerStatusWidget::update));
    connect(m_powerInter, &DBusPower::BatteryStateChanged, this, static_cast<void (PowerStatusWidget::*)()>(&PowerStatusWidget::update));
    connect(m_powerInter, &DBusPower::OnBatteryChanged, this, static_cast<void (PowerStatusWidget::*)()>(&PowerStatusWidget::update));
}

void PowerStatusWidget::refreshIcon()
{
    update();
}

QSize PowerStatusWidget::sizeHint() const
{
    return QSize(60, 26);
}

void PowerStatusWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const QPixmap icon = getBatteryIcon();
    const auto ratio = devicePixelRatioF();

    QPainter painter(this);
    const QRectF &rf = QRectF(rect());
    const QRectF &rfp = QRectF(icon.rect());
    
    // 绘制图标，调整位置为左侧
    QPointF iconPos = rf.center() - rfp.center() / ratio;
    iconPos.setX(5 * ratio); // 左侧留出一些空间，考虑设备像素比
    painter.drawPixmap(iconPos, icon);

    // 绘制电池百分比
    const BatteryPercentageMap data = m_powerInter->batteryPercentage();
    const uint value = qMin(100.0, qMax(0.0, data.value("Display")));
    const int percentage = std::round(value);
    
    // 保持固定字体大小，避免文字超出范围
    painter.setFont(QFont("Source Han Sans SC", 10));
    painter.setPen(QColor("#ffffff"));
    
    // 计算文本位置，显示在图标右侧，增加间距
    QRect textRect = rect();
    textRect.adjust(25, 0, 0, 0); // 左侧留出图标的空间，增加间距
    painter.drawText(textRect, Qt::AlignVCenter, QString("%1%").arg(percentage));
}

QPixmap PowerStatusWidget::getBatteryIcon()
{
    const BatteryPercentageMap data = m_powerInter->batteryPercentage();
    const uint value = qMin(100.0, qMax(0.0, data.value("Display")));
    const int percentage = std::round(value);
    const int batteryState = m_powerInter->batteryState()["Display"];
    const bool plugged = !m_powerInter->onBattery();

    QString percentageStr;
    if (percentage < 10 && percentage >= 0) {
        percentageStr = "000";
    } else if (percentage < 30) {
        percentageStr = "020";
    } else if (percentage < 50) {
        percentageStr = "040";
    } else if (percentage < 70) {
        percentageStr = "060";
    } else if (percentage < 90) {
        percentageStr = "080";
    } else if (percentage <= 100){
        percentageStr = "100";
    } else {
        percentageStr = "000";
    }

    const QString iconStr = QString("battery-%1-%2")
                                .arg(percentageStr)
                                .arg(plugged ? "plugged-symbolic" : "symbolic");
    const auto ratio = devicePixelRatioF();
    QPixmap pix = QIcon::fromTheme(iconStr).pixmap(QSize(16, 16) * ratio);
    pix.setDevicePixelRatio(ratio);

    return pix;
}
