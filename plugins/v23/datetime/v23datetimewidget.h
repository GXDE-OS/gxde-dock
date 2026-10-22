// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.

#ifndef V23DATETIMEWIDGET_H
#define V23DATETIMEWIDGET_H

#include <QWidget>

// Deepin V23 taskbar clock: time above a smaller date on top/bottom docks,
// time (and AM/PM) over a short date on left/right docks.
class V23DatetimeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit V23DatetimeWidget(QWidget *parent = nullptr);

    bool is24HourFormat() const { return m_24HourFormat; }
    // refresh the text; emits requestUpdateGeometry when the size changes
    void updateDateTime();

signals:
    void requestUpdateGeometry() const;

public slots:
    void set24HourFormat(const bool value);

protected:
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    bool horizontal() const;
    bool twoRows() const;
    QFont timeFont() const;
    QFont dateFont() const;
    QStringList lines() const;

private:
    bool m_24HourFormat;
    QStringList m_lines;
    QSize m_lastHint;
};

#endif // V23DATETIMEWIDGET_H
