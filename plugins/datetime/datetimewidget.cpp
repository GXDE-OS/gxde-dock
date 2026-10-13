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

#include "datetimewidget.h"
#include "constants.h"

#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QSvgRenderer>
#include <QMouseEvent>

#define PLUGIN_STATE_KEY    "enable"

// 高效/经典模式下，任务栏条目尺寸由 dock 的"大小"设置决定：
// 上下方向的条目高度：小(30) -> 36，中(36) -> 43，大(48) -> 58；
// 左右方向的条目宽度：小(30) -> 50，中(36) -> 60，大(48) -> 80。
// 条目尺寸小于下列阈值时视为"小"尺寸。
#define SMALL_PANEL_HEIGHT  40
#define SMALL_PANEL_WIDTH   55

DatetimeWidget::DatetimeWidget(QWidget *parent)
    : QWidget(parent)
    , m_24HourFormat(true)
    , m_lineCount(0)
{

}

void DatetimeWidget::set24HourFormat(const bool value)
{
    if (m_24HourFormat == value) {
        return;
    }

    m_24HourFormat = value;

    m_cachedTime.clear();
    update();

    // 12/24小时制的文字宽度不同，需要失效 sizeHint 缓存(Qt 不会因内容变化
    // 自动失效)，否则面板拿到的仍是旧的条目尺寸
    updateGeometry();

    if (isVisible()) {
        emit requestUpdateGeometry();
    }
}

QSize DatetimeWidget::sizeHint() const
{
    const QStringList lines = textLines();
    const QFontMetrics fm(font());

    int maxWidth = 0;
    for (const QString &line : lines)
        maxWidth = qMax(maxWidth, fm.boundingRect(line).width());

    // 上下方向条目的高度由面板按任务栏大小强制指定，这里只需给出一行文字的
    // 高度；左右方向条目的高度取 sizeHint，必须能容纳所有文字行，否则多行
    // 文字会被裁剪导致显示不完整
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    const bool horizontal = (position == Dock::Top || position == Dock::Bottom);
    const int height = horizontal ? fm.height() + 10
                                  : lines.count() * fm.lineSpacing() + 10;

    return QSize(maxWidth + 20, height);
}

bool DatetimeWidget::panelIsSmallSize() const
{
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();

    // 上下方向由条目高度判断，左右方向由条目宽度判断
    if (position == Dock::Top || position == Dock::Bottom)
        return height() > 0 && height() <= SMALL_PANEL_HEIGHT;

    return width() > 0 && width() <= SMALL_PANEL_WIDTH;
}

bool DatetimeWidget::textFits(const QString &text) const
{
    return QFontMetrics(font()).boundingRect(text).width() <= width();
}

QStringList DatetimeWidget::textLines() const
{
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    const QDateTime current = QDateTime::currentDateTime();
    // 按当前语言环境取实际渲染的文字，保证宽度测量准确
    const QString timeText = current.toString(m_24HourFormat ? "hh:mm" : "hh:mm AP");
    const QString dateText = current.toString("yyyy/MM/dd");

    QStringList lines;
    if (position == Dock::Top || position == Dock::Bottom) {
        // 任务栏尺寸为"小"时条目高度不足以容纳两行文字，
        // 日期与时间在一行显示，避免文字互相挤压、显得丑陋
        if (panelIsSmallSize())
            lines << dateText + " " + timeText;
        else
            lines << timeText << dateText;
        return lines;
    }

    // 任务栏位于左右两侧时条目很窄
    if (panelIsSmallSize()) {
        // 小尺寸只显示时间，12小时制的上下午标记放不下时单独一行
        if (m_24HourFormat || textFits(timeText))
            lines << timeText;
        else
            lines << current.toString("hh:mm") << current.toString("AP");
        return lines;
    }

    // 中/大尺寸显示时间和日期，放不下的行自动拆分，保证显示完整
    if (m_24HourFormat || textFits(timeText))
        lines << timeText;
    else
        lines << current.toString("hh:mm") << current.toString("AP");

    if (textFits(dateText))
        lines << dateText;
    else
        lines << current.toString("yyyy") << current.toString("/MM/dd");

    return lines;
}

void DatetimeWidget::resizeEvent(QResizeEvent *e)
{
    m_cachedTime.clear();

    // 条目尺寸变化时(如切换任务栏大小、位置、显示模式)，文字行数可能发生
    // 改变，此时需要通知面板重新计算条目尺寸，避免文字被裁剪。
    // updateGeometry() 用于失效 sizeHint 缓存(Qt 不会因控件尺寸变化而自动失效)，
    // 否则面板拿到的仍是旧的条目尺寸
    const int lineCount = textLines().count();
    if (lineCount != m_lineCount) {
        m_lineCount = lineCount;
        updateGeometry();
        emit requestUpdateGeometry();
    }

    QWidget::resizeEvent(e);
}

void DatetimeWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    const auto ratio = devicePixelRatioF();
    const Dock::DisplayMode displayMode = qApp->property(PROP_DISPLAY_MODE).value<Dock::DisplayMode>();
    const QDateTime current = QDateTime::currentDateTime();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (displayMode == Dock::Efficient || displayMode == Dock::Classic)
    {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, textLines().join("\n"));
        return;
    }

    // use language Chinese to fix can not find image resources which will be drawn
    const QString currentTimeString = QLocale(QLocale::Chinese, QLocale::system().country())
            .toString(current, m_24HourFormat ? "hhmm" : "hhmma");

    // check cache valid
    if (m_cachedTime != currentTimeString)
    {
        m_cachedTime = currentTimeString;

        // draw new pixmap
        m_cachedTime = currentTimeString;
        m_cachedIcon = QPixmap(size() * ratio);
        m_cachedIcon.fill(Qt::transparent);
        m_cachedIcon.setDevicePixelRatio(ratio);
        QPainter p(&m_cachedIcon);

        // draw fashion mode datetime plugin
        const int perfectIconSize = qMin(width(), height()) * 0.8;
        const QRect r = rect();

        // draw background
        QPixmap background = loadSvg(":/icons/resources/icons/background.svg", QSize(perfectIconSize, perfectIconSize));
        // 居中偏移必须用 pixmap 自身的 DPR（修复 HDPI 下图标偏左上）
        const QPoint backgroundOffset = r.center() - background.rect().center() / background.devicePixelRatioF();
        p.drawPixmap(backgroundOffset, background);

        const int bigNumHeight = perfectIconSize / 2.5;
        const int bigNumWidth = double(bigNumHeight) * 8 / 18;
        const int smallNumHeight = bigNumHeight / 2;
        const int smallNumWidth = double(smallNumHeight) * 5 / 9;

        // draw big num 1
        const QString bigNum1Path = QString(":/icons/resources/icons/big%1.svg").arg(currentTimeString[0]);
        const QPixmap bigNum1 = loadSvg(bigNum1Path, QSize(bigNumWidth, bigNumHeight));
        const QPoint bigNum1Offset = backgroundOffset + QPoint(perfectIconSize / 2 - bigNumWidth * 2 + 1, perfectIconSize / 2 - bigNumHeight / 2);
        p.drawPixmap(bigNum1Offset, bigNum1);

        // draw big num 2
        const QString bigNum2Path = QString(":/icons/resources/icons/big%1.svg").arg(currentTimeString[1]);
        const QPixmap bigNum2 = loadSvg(bigNum2Path, QSize(bigNumWidth, bigNumHeight));
        const QPoint bigNum2Offset = bigNum1Offset + QPoint(bigNumWidth + 1, 0);
        p.drawPixmap(bigNum2Offset, bigNum2);

        if (!m_24HourFormat)
        {
            // draw small num 1
            const QString smallNum1Path = QString(":/icons/resources/icons/small%1.svg").arg(currentTimeString[2]);
            const QPixmap smallNum1 = loadSvg(smallNum1Path, QSize(smallNumWidth, smallNumHeight));
            const QPoint smallNum1Offset = bigNum2Offset + QPoint(bigNumWidth + 2, 1);
            p.drawPixmap(smallNum1Offset, smallNum1);

            // draw small num 2
            const QString smallNum2Path = QString(":/icons/resources/icons/small%1.svg").arg(currentTimeString[3]);
            const QPixmap smallNum2 = loadSvg(smallNum2Path, QSize(smallNumWidth, smallNumHeight));
            const QPoint smallNum2Offset = smallNum1Offset + QPoint(smallNumWidth + 1, 0);
            p.drawPixmap(smallNum2Offset, smallNum2);

            // draw am/pm tips
            const int tips_width = (smallNumWidth * 2 + 2) & ~0x1;
            const int tips_height = tips_width / 2;

            QPixmap tips;
            if (current.time().hour() > 11)
                tips = loadSvg(":/icons/resources/icons/tips-pm.svg", QSize(tips_width, tips_height));
            else
                tips = loadSvg(":/icons/resources/icons/tips-am.svg", QSize(tips_width, tips_height));

            const QPoint tipsOffset = bigNum2Offset + QPoint(bigNumWidth + 2, bigNumHeight - tips_height);
            p.drawPixmap(tipsOffset, tips);
        } else {
            // draw small num 1
            const QString smallNum1Path = QString(":/icons/resources/icons/small%1.svg").arg(currentTimeString[2]);
            const QPixmap smallNum1 = loadSvg(smallNum1Path, QSize(smallNumWidth, smallNumHeight));
            const QPoint smallNum1Offset = bigNum2Offset + QPoint(bigNumWidth + 2, smallNumHeight);
            p.drawPixmap(smallNum1Offset, smallNum1);

            // draw small num 2
            const QString smallNum2Path = QString(":/icons/resources/icons/small%1.svg").arg(currentTimeString[3]);
            const QPixmap smallNum2 = loadSvg(smallNum2Path, QSize(smallNumWidth, smallNumHeight));
            const QPoint smallNum2Offset = smallNum1Offset + QPoint(smallNumWidth + 1, 0);
            p.drawPixmap(smallNum2Offset, smallNum2);
        }
    }

    // draw cached fashion mode time item
    // 居中偏移必须用 pixmap 自身的 DPR（修复 HDPI 下图标偏左上）
    painter.drawPixmap(rect().center() - m_cachedIcon.rect().center() / m_cachedIcon.devicePixelRatioF(), m_cachedIcon);
}

const QPixmap DatetimeWidget::loadSvg(const QString &fileName, const QSize size)
{
    const auto ratio = devicePixelRatioF();

    QPixmap pixmap(size * ratio);
    QSvgRenderer renderer(fileName);
    pixmap.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&pixmap);
    renderer.render(&painter);
    painter.end();

    pixmap.setDevicePixelRatio(ratio);

    return pixmap;
}
