// SPDX-FileCopyrightText: 2019 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.

#ifndef V23CALENDARWIDGET_H
#define V23CALENDARWIDGET_H

#include <QAbstractButton>
#include <QDate>
#include <QWidget>

class QLabel;
class QTimer;
class QDBusPendingCallWatcher;

// Buttons of the V23 calendar popup, painted to match its theme
class V23CalendarButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum Kind {
        PreviousPage,
        NextPage,
        Link,       // highlighted text, e.g. "Today"
        Jump,       // "Open the calendar"
    };

    explicit V23CalendarButton(Kind kind, const QString &text = QString(), QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    Kind m_kind;
    bool m_hover;
};

// The big day of month followed by "/ Oct", sharing one baseline
class V23DateTitle : public QWidget
{
    Q_OBJECT

public:
    explicit V23DateTitle(QWidget *parent = nullptr);

    void setDate(const QDate &date);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QFont dayFont() const;
    QFont monthFont() const;
    int dayBaseline() const;

private:
    QDate m_date;
};

// Weekday header and the 6x7 day grid of the displayed month
class V23MonthView : public QWidget
{
    Q_OBJECT

public:
    explicit V23MonthView(QWidget *parent = nullptr);

    void setMonth(const QDate &displayed, const QDate &selected);

signals:
    void dateClicked(const QDate &date) const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    QDate firstVisibleDate() const;
    QRect cellRect(int index) const;
    int indexAt(const QPoint &pos) const;

private:
    QDate m_displayed;
    QDate m_selected;
    int m_hoverIndex;
    int m_pressIndex;
};

// The popup shown when the V23 taskbar clock is clicked: the selected date
// with its lunar date on the left, a month calendar on the right.
class V23CalendarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit V23CalendarWidget(QWidget *parent = nullptr);

signals:
    void jumpButtonClicked() const;

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void setSelectedDate(const QDate &date);
    void backToday();
    void showMonthOffset(int months);
    void requestLunar(const QDate &date);
    void onLunarReply(QDBusPendingCallWatcher *watcher);
    void updateColors();
    void setElidedText(QLabel *label, const QString &text);

private:
    V23DateTitle *m_title;
    QLabel *m_weekLabel;
    QLabel *m_dateLabel;
    QWidget *m_lunarBox;
    QLabel *m_lunarLabel;
    QLabel *m_lunarDetailLabel;
    V23CalendarButton *m_jumpButton;

    QLabel *m_headLabel;
    V23CalendarButton *m_todayButton;
    V23CalendarButton *m_previousButton;
    V23CalendarButton *m_nextButton;
    V23MonthView *m_monthView;

    QDate m_selected;
    QDate m_displayed;
    QDate m_today;
    QDate m_lunarRequest;
    int m_deltaSum;
    QTimer *m_dayTimer;
};

#endif // V23CALENDARWIDGET_H
