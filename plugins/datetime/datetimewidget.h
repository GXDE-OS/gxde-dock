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

#ifndef DATETIMEWIDGET_H
#define DATETIMEWIDGET_H

#include <QWidget>

class DatetimeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DatetimeWidget(QWidget *parent = 0);

    bool is24HourFormat() const { return m_24HourFormat; }

signals:
    void requestUpdateGeometry() const;

public slots:
    void set24HourFormat(const bool value);

private:
    QSize sizeHint() const;
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *e);

    const QPixmap loadSvg(const QString &fileName, const QSize size);

    // 高效/经典模式下，任务栏尺寸设为"小"时条目尺寸不足以容纳
    // 两行文字，时间与日期需要合并为一行显示(左右方向则只显示时间)
    bool panelIsSmallSize() const;
    // 计算高效/经典模式下需要显示的文字行，保证每行都能完整显示在条目中
    QStringList textLines() const;
    // 判断一行文字是否能完整显示在当前条目宽度内
    bool textFits(const QString &text) const;

private:
    QPixmap m_cachedIcon;
    QString m_cachedTime;
    bool m_24HourFormat;
    int m_lineCount;
};

#endif // DATETIMEWIDGET_H
