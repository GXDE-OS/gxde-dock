// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.

#include "v23datetimewidget.h"
#include "constants.h"

#include <QApplication>
#include <QDateTime>
#include <QPainter>

V23DatetimeWidget::V23DatetimeWidget(QWidget *parent)
    : QWidget(parent)
    , m_24HourFormat(true)
{
    m_lines = lines();
}

void V23DatetimeWidget::set24HourFormat(const bool value)
{
    if (m_24HourFormat == value)
        return;

    m_24HourFormat = value;
    updateDateTime();
}

void V23DatetimeWidget::updateDateTime()
{
    const QStringList current = lines();
    if (current != m_lines) {
        m_lines = current;
        update();
    }

    // e.g. 9:59 -> 10:00 in 12-hour format, or a dock position change
    const QSize hint = sizeHint();
    if (hint != m_lastHint) {
        m_lastHint = hint;
        updateGeometry();
        emit requestUpdateGeometry();
    }
}

bool V23DatetimeWidget::horizontal() const
{
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    return position == Dock::Top || position == Dock::Bottom;
}

bool V23DatetimeWidget::twoRows() const
{
    // a small top/bottom dock only fits the time
    return !horizontal() || height() <= 0 || height() >= 30;
}

QFont V23DatetimeWidget::timeFont() const
{
    QFont font = this->font();
    const int thickness = horizontal() ? height() : width();
    const int size = thickness > 0 ? thickness : 40;
    font.setPixelSize(horizontal() ? qBound(12, qRound(size * 0.34), 16)
                                   : qBound(11, qRound(size * 0.26), 15));
    font.setWeight(QFont::Medium);
    return font;
}

QFont V23DatetimeWidget::dateFont() const
{
    QFont font = this->font();
    font.setPixelSize(timeFont().pixelSize() - 3);
    return font;
}

QStringList V23DatetimeWidget::lines() const
{
    const QDateTime current = QDateTime::currentDateTime();

    if (horizontal()) {
        QStringList result { current.toString(m_24HourFormat ? "hh:mm" : "hh:mm AP") };
        if (twoRows())
            result << current.toString("yyyy/MM/dd");
        return result;
    }

    // the dock is narrow: AM/PM gets its own row, the year is dropped
    // ("hh" is only 12-hour in a format that also contains "AP")
    QStringList result { current.toString("hh:mm") };
    if (!m_24HourFormat) {
        result.first() = current.toString("hh:mm AP").section(' ', 0, 0);
        result << current.toString("AP");
    }
    result << current.toString("MM/dd");
    return result;
}

QSize V23DatetimeWidget::sizeHint() const
{
    const QFontMetrics timeMetrics(timeFont());
    const QFontMetrics dateMetrics(dateFont());
    const QStringList text = lines();

    if (horizontal()) {
        // the panel forces the height of top/bottom items
        int width = timeMetrics.horizontalAdvance(text.first());
        if (text.size() > 1)
            width = qMax(width, dateMetrics.horizontalAdvance(text.last()));
        return QSize(width + 16, height());
    }

    // and the width of left/right items
    const int height = timeMetrics.height() * (text.size() - 1) + dateMetrics.height();
    return QSize(width(), height + 10);
}

void V23DatetimeWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    // fonts and the number of rows follow the item thickness
    updateDateTime();
}

void V23DatetimeWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Dock::isV23Light() ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white));

    const QFont time = timeFont();
    const QFont date = dateFont();
    const int timeHeight = QFontMetrics(time).height();
    const int dateHeight = QFontMetrics(date).height();

    if (m_lines.size() == 1) {
        painter.setFont(time);
        painter.drawText(rect(), Qt::AlignCenter, m_lines.first());
        return;
    }

    // every row but the last one is drawn with the time font
    const int total = timeHeight * (m_lines.size() - 1) + dateHeight;
    int top = (height() - total) / 2;

    painter.setFont(time);
    for (int i = 0; i < m_lines.size() - 1; ++i) {
        painter.drawText(QRect(0, top, width(), timeHeight), Qt::AlignCenter, m_lines.at(i));
        top += timeHeight;
    }

    painter.setFont(date);
    painter.setOpacity(0.8);
    painter.drawText(QRect(0, top, width(), dateHeight), Qt::AlignCenter, m_lines.last());
}
