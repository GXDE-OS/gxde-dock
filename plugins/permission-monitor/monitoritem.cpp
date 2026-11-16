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

#include "monitoritem.h"

#include <QHideEvent>
#include <QIcon>
#include <QPainter>
#include <QShowEvent>
#include <QSvgRenderer>

#include <algorithm>

namespace {

// 占用指示为红色警示图标，深浅色面板下均清晰可辨
const QColor inUseColor(0xF4, 0x43, 0x36);
// 闪烁的另一半为白色（V23 浅色主题下 dock 会自动把白色单色内容描为深色）
const QColor blinkColor(0xFF, 0xFF, 0xFF);
// 闪烁节奏：每 500ms 交替一次，一个白/红周期为 1 秒
const int blinkInterval = 500;

// 按 devicePixelRatio 渲染 svg 文件，避免高分屏下图标发糊
QPixmap renderSvgFile(const QString &filePath, const QSize &size, qreal ratio)
{
    QSvgRenderer renderer(filePath);
    if (!renderer.isValid()) {
        return QPixmap();
    }

    QPixmap pixmap(size * ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(QPointF(0, 0), size));
    painter.end();

    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

QPixmap themePixmap(const QString &iconName, int size, qreal ratio)
{
    return QIcon::fromTheme(iconName).pixmap(QSize(size, size), ratio);
}

// 保留 alpha 形状，将图标整体染为指定颜色
QPixmap tintPixmap(const QPixmap &source, const QColor &color)
{
    QPixmap result(source.size());
    result.setDevicePixelRatio(source.devicePixelRatio());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRect(QPoint(0, 0), source.size()), color);
    painter.end();

    return result;
}

} // namespace

MonitorItem::MonitorItem(const QStringList &themeIconNames,
                         const QString &fallbackIconPath,
                         QWidget *parent)
    : QWidget(parent)
    , m_displayMode(Dock::Efficient)
    , m_themeIconNames(themeIconNames)
    , m_fallbackIconPath(fallbackIconPath)
    , m_currentIcon(0)
    , m_blinkTimer(new QTimer(this))
{
    m_blinkTimer->setInterval(blinkInterval);
    connect(m_blinkTimer, &QTimer::timeout, this, &MonitorItem::blink);
}

void MonitorItem::setDockDisplayMode(const Dock::DisplayMode mode)
{
    m_displayMode = mode;

    // 两种模式的图标尺寸期望不同，通知父布局重新计算
    updateGeometry();

    updateIcon();
}

void MonitorItem::refreshIcon()
{
    updateIcon();
}

void MonitorItem::blink()
{
    m_currentIcon = (m_currentIcon + 1) % 2;
    m_icon = m_icons[m_currentIcon];

    update();
}

void MonitorItem::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    // 与 sounditem 相同的居中绘制方式（考虑 devicePixelRatio）
    painter.drawPixmap(rect().center() - m_icon.rect().center() / m_icon.devicePixelRatioF(), m_icon);
}

void MonitorItem::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    updateIcon();
}

void MonitorItem::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    // 仅在图标显示期间闪烁；从红色开始交替
    m_currentIcon = 0;
    m_icon = m_icons[m_currentIcon];
    m_blinkTimer->start();
}

void MonitorItem::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);

    m_blinkTimer->stop();
}

QSize MonitorItem::sizeHint() const
{
    // Fashion 模式彩色图标较大，symbolic 模式为 16px 小图标；
    // 两种模式的尺寸期望不同，供容器布局与 dock 分配空间使用
    if (m_displayMode == Dock::Efficient || m_displayMode == Dock::V23) {
        return QSize(16, 16);
    }

    return QSize(38, 38);
}

void MonitorItem::updateIcon()
{
    // Efficient/V23 模式优先使用单色 -symbolic 图标作为基底，
    // Fashion 模式使用彩色图标；占用指示最终统一染为红/白两色闪烁。
    // Fashion 模式下图标随容器高度缩放（与其它插件图标一致），
    // 但不超过自身宽度（并排摆放时宽度是较小的一边）
    const bool symbolicMode = (m_displayMode == Dock::Efficient || m_displayMode == Dock::V23);
    const int size = symbolicMode ? 16
                                  : std::min(width(), int(height() * 0.8));

    QPixmap base;

    QString iconName;
    for (const QString &candidate : m_themeIconNames) {
        const QString name = symbolicMode ? candidate + QStringLiteral("-symbolic")
                                          : candidate;
        if (QIcon::hasThemeIcon(name)) {
            iconName = name;
            break;
        }
    }

    if (!iconName.isEmpty()) {
        base = themePixmap(iconName, size, devicePixelRatioF());
    }

    // 主题图标可能取不到像素图（主题不完整），退回内置 svg 图标
    if (base.isNull()) {
        base = renderSvgFile(m_fallbackIconPath, QSize(size, size), devicePixelRatioF());
    }

    if (base.isNull()) {
        m_icons[0] = QPixmap();
        m_icons[1] = QPixmap();
    } else {
        // 预生成白/红两份像素图，闪烁时仅切换引用
        m_icons[0] = tintPixmap(base, inUseColor);
        m_icons[1] = tintPixmap(base, blinkColor);
    }

    m_icon = m_icons[m_currentIcon];

    update();
}
