// SPDX-FileCopyrightText: 2019 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Adapted from dde-dock, which licenses under LGPLv3.
// Slightly modified that the lunar year system is adapted to GXDE's DBus, not Deepin's service that
// does not exist on GXDE.

#include "v23calendarwidget.h"
#include "constants.h"

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>

namespace {
constexpr int kDaysInWeek = 7;
constexpr int kWeeks = 6;
constexpr int kWeekHeaderHeight = 24;
constexpr int kRoundCorner = 8;
constexpr int kRectMargins = 1;
constexpr int kRectPenWidth = 2;
constexpr int kTurnPageDelta = 120;

// "Open the calendar" icon of dde-tray-loader (open-calendar_16px.svg)
const char kOpenCalendarSvg[] = R"svg(<svg width="16" height="16" viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
<g fill="%1" fill-opacity="0.7" transform="translate(1, 4.038)">
<path d="M1,4.962 C1.552,4.962 2,4.514 2,3.962 C2,3.41 1.552,2.962 1,2.962 C0.448,2.962 0,3.41 0,3.962 C0,4.514 0.448,4.962 1,4.962 Z M4,4.962 C4.552,4.962 5,4.514 5,3.962 C5,3.41 4.552,2.962 4,2.962 C3.448,2.962 3,3.41 3,3.962 C3,4.514 3.448,4.962 4,4.962 Z M11.144,0.207 L14.179,3.243 C14.57,3.633 14.57,4.266 14.179,4.657 L11.144,7.692 C10.895,7.941 10.508,7.966 10.232,7.767 L10.144,7.692 C9.868,7.416 9.868,6.968 10.144,6.692 L12.886,3.95 L10.143,1.207 C9.895,0.959 9.87,0.571 10.069,0.295 L10.143,0.207 C10.42,-0.069 10.868,-0.069 11.144,0.207 Z M10,2.962 C10.552,2.962 11,3.41 11,3.962 C11,4.514 10.552,4.962 10,4.962 L7,4.962 C6.448,4.962 6,4.514 6,3.962 C6,3.41 6.448,2.962 7,2.962 L10,2.962 Z"/>
</g>
</svg>)svg";

// The popup panel follows the V23 color theme, on X11 as on Wayland
bool lightPopup()
{
    return Dock::isV23Light();
}

QColor textColor()
{
    return lightPopup() ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white);
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

QColor highlightColor() {
    return lightPopup() ? QColor(0x00, 0x81, 0xff) : QColor(0x02, 0x4c, 0xca);
}

QFont pixelFont(const QFont &base, int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font = base;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

bool isChinese()
{
    return QLocale::system().language() == QLocale::Chinese;
}

QString monthName(const QDate &date)
{
    return QLocale::system().standaloneMonthName(date.month(), QLocale::ShortFormat);
}
}

V23CalendarButton::V23CalendarButton(Kind kind, const QString &text, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
    , m_hover(false)
{
    setText(text);
    setFocusPolicy(Qt::NoFocus);
    setCursor(m_kind == Link ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFont(pixelFont(font(), 14));
}

QSize V23CalendarButton::sizeHint() const
{
    const QFontMetrics fm(font());
    switch (m_kind) {
    case PreviousPage:
    case NextPage:
        return QSize(30, 30);
    case Link:
        return QSize(fm.horizontalAdvance(text()) + 8, 30);
    case Jump:
        // 10 + icon 16 + 5 + text + 10
        return QSize(fm.horizontalAdvance(text()) + 41, 36);
    }
    return QSize();
}

void V23CalendarButton::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(e);
}

void V23CalendarButton::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(e);
}

void V23CalendarButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor text = textColor();

    if (m_kind == Link) {
        QColor color = highlightColor();
        if (isDown())
            color = color.darker(120);
        painter.setPen(color);
        painter.setFont(font());
        painter.drawText(rect(), Qt::AlignCenter, this->text());
        return;
    }

    qreal background = 0;
    if (isDown())
        background = 0.15;
    else if (m_hover)
        background = 0.1;
    else if (m_kind == Jump)
        background = 0.05;

    if (background > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(text, background));
        painter.drawRoundedRect(rect(), kRoundCorner, kRoundCorner);
    }

    if (m_kind == Jump) {
        const QColor color = isDown() ? highlightColor() : text;
        QSvgRenderer icon(QByteArray(kOpenCalendarSvg).replace("%1", color.name().toLatin1()));
        icon.render(&painter, QRectF(10, (height() - 16) / 2.0, 16, 16));

        painter.setPen(color);
        painter.setFont(font());
        const QRect textRect = rect().adjusted(31, 0, -10, 0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                         fontMetrics().elidedText(this->text(), Qt::ElideRight, textRect.width()));
        return;
    }

    // page chevrons
    const QPointF c = QRectF(rect()).center();
    const qreal d = m_kind == PreviousPage ? 1 : -1;
    QPainterPath chevron;
    chevron.moveTo(c + QPointF(2.5 * d, -5));
    chevron.lineTo(c + QPointF(-2.5 * d, 0));
    chevron.lineTo(c + QPointF(2.5 * d, 5));
    painter.setPen(QPen(text, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(chevron);
}

V23DateTitle::V23DateTitle(QWidget *parent)
    : QWidget(parent)
{
}

void V23DateTitle::setDate(const QDate &date)
{
    m_date = date;
    update();
}

QFont V23DateTitle::dayFont() const
{
    QFont font = pixelFont(this->font(), 96, QFont::Light);
    font.setFamilies({ QStringLiteral("Noto Sans CJK SC"), this->font().family() });
    return font;
}

int V23DateTitle::dayBaseline() const
{
    // round digits overshoot the cap height, so measure their outline
    QPainterPath outline;
    outline.addText(0, 0, dayFont(), QStringLiteral("0123456789"));
    return qCeil(-outline.boundingRect().top()) + 1;
}

QFont V23DateTitle::monthFont() const
{
    return pixelFont(font(), 20);
}

QSize V23DateTitle::sizeHint() const
{
    // digits have no descent; leave room for the month text's
    return QSize(200, dayBaseline() + QFontMetrics(monthFont()).descent());
}

void V23DateTitle::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    if (!m_date.isValid())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(textColor());

    const QFont day = dayFont();
    const QString dayText = QString::number(m_date.day());
    const QFontMetrics dayMetrics(day);
    const int baseline = dayBaseline();
    // start the digits at the left edge rather than at their side bearing;
    // the outline is exact where tightBoundingRect is not for light weights
    QPainterPath outline;
    outline.addText(0, 0, day, dayText);
    const int x = qCeil(-outline.boundingRect().left()) + 1;

    painter.setFont(day);
    painter.drawText(x, baseline, dayText);

    painter.setFont(monthFont());
    painter.drawText(x + dayMetrics.horizontalAdvance(dayText) + 6, baseline, "/ " + monthName(m_date));
}

V23MonthView::V23MonthView(QWidget *parent)
    : QWidget(parent)
    , m_hoverIndex(-1)
    , m_pressIndex(-1)
{
    setMouseTracking(true);
    setFont(pixelFont(font(), 14));
}

void V23MonthView::setMonth(const QDate &displayed, const QDate &selected)
{
    m_displayed = displayed;
    m_selected = selected;
    update();
}

QDate V23MonthView::firstVisibleDate() const
{
    const QDate first(m_displayed.year(), m_displayed.month(), 1);
    const int firstDay = QLocale::system().firstDayOfWeek();
    return first.addDays(-((first.dayOfWeek() - firstDay + kDaysInWeek) % kDaysInWeek));
}

QRect V23MonthView::cellRect(int index) const
{
    const qreal w = width() / qreal(kDaysInWeek);
    const qreal h = (height() - kWeekHeaderHeight) / qreal(kWeeks);
    const int column = index % kDaysInWeek;
    const int row = index / kDaysInWeek;
    return QRectF(column * w, kWeekHeaderHeight + row * h, w, h).toRect();
}

int V23MonthView::indexAt(const QPoint &pos) const
{
    for (int i = 0; i < kDaysInWeek * kWeeks; ++i) {
        if (cellRect(i).contains(pos))
            return i;
    }
    return -1;
}

void V23MonthView::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    if (!m_displayed.isValid())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font());

    const QColor text = textColor();
    const QColor highlight = highlightColor();
    const QLocale locale = QLocale::system();
    const int firstDay = locale.firstDayOfWeek();

    // weekday header: 一 二 三 ... in Chinese, Mon Tue ... elsewhere
    painter.setPen(text);
    const QLocale::FormatType format = isChinese() ? QLocale::NarrowFormat : QLocale::ShortFormat;
    for (int i = 0; i < kDaysInWeek; ++i) {
        const int day = (firstDay - 1 + i) % kDaysInWeek + 1;
        const QRect r = cellRect(i);
        painter.drawText(QRect(r.x(), 0, r.width(), kWeekHeaderHeight), Qt::AlignCenter,
                         locale.standaloneDayName(day, format));
    }

    const QDate today = QDate::currentDate();
    QDate date = firstVisibleDate();
    for (int i = 0; i < kDaysInWeek * kWeeks; ++i, date = date.addDays(1)) {
        const QRect cell = cellRect(i);
        const int side = qMin(cell.width(), cell.height());
        const QRectF square(cell.x() + (cell.width() - side) / 2, cell.y() + (cell.height() - side) / 2, side, side);
        const QRectF plate = square.marginsRemoved(QMarginsF(kRectMargins, kRectMargins, kRectMargins, kRectMargins));

        painter.setOpacity(1);
        if (i == m_hoverIndex) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(text, 0.1));
            painter.drawRoundedRect(plate, kRoundCorner, kRoundCorner);
        }

        if (date == today) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(highlight);
            painter.drawRoundedRect(plate, kRoundCorner, kRoundCorner);
            painter.setPen(Qt::white);
        } else if (date == m_selected) {
            painter.setPen(QPen(highlight, kRectPenWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(square.adjusted(kRectPenWidth, kRectPenWidth, -kRectPenWidth, -kRectPenWidth),
                                    kRoundCorner, kRoundCorner);
            painter.setPen(text);
        } else {
            painter.setPen(text);
            if (date.month() != m_displayed.month())
                painter.setOpacity(0.3);
        }

        painter.drawText(square, Qt::AlignCenter, QString::number(date.day()));
    }
}

void V23MonthView::mouseMoveEvent(QMouseEvent *e)
{
    const int index = indexAt(e->position().toPoint());
    if (index != m_hoverIndex) {
        m_hoverIndex = index;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void V23MonthView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_pressIndex = indexAt(e->position().toPoint());
    QWidget::mousePressEvent(e);
}

void V23MonthView::mouseReleaseEvent(QMouseEvent *e)
{
    const int index = indexAt(e->position().toPoint());
    if (e->button() == Qt::LeftButton && index >= 0 && index == m_pressIndex)
        emit dateClicked(firstVisibleDate().addDays(index));
    m_pressIndex = -1;
    QWidget::mouseReleaseEvent(e);
}

void V23MonthView::leaveEvent(QEvent *e)
{
    m_hoverIndex = -1;
    update();
    QWidget::leaveEvent(e);
}

V23CalendarWidget::V23CalendarWidget(QWidget *parent)
    : QWidget(parent)
    , m_title(new V23DateTitle(this))
    , m_weekLabel(new QLabel(this))
    , m_dateLabel(new QLabel(this))
    , m_lunarBox(new QWidget(this))
    , m_lunarLabel(new QLabel(m_lunarBox))
    , m_lunarDetailLabel(new QLabel(m_lunarBox))
    , m_jumpButton(new V23CalendarButton(V23CalendarButton::Jump, tr("Open the calendar"), this))
    , m_headLabel(new QLabel(this))
    , m_todayButton(new V23CalendarButton(V23CalendarButton::Link, tr("Today"), this))
    , m_previousButton(new V23CalendarButton(V23CalendarButton::PreviousPage, QString(), this))
    , m_nextButton(new V23CalendarButton(V23CalendarButton::NextPage, QString(), this))
    , m_monthView(new V23MonthView(this))
    , m_deltaSum(0)
    , m_dayTimer(new QTimer(this))
{
    setObjectName("datetime-calendar");
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(500, 300);

    m_title->setFixedHeight(m_title->sizeHint().height());

    m_weekLabel->setFont(pixelFont(font(), 17, QFont::Medium));
    m_dateLabel->setFont(pixelFont(font(), 17, QFont::Medium));
    m_lunarLabel->setFont(pixelFont(font(), 14));
    m_lunarDetailLabel->setFont(pixelFont(font(), 14));
    m_jumpButton->setFixedSize(m_jumpButton->sizeHint());

    auto *lunarLayout = new QVBoxLayout(m_lunarBox);
    lunarLayout->setSpacing(0);
    lunarLayout->setContentsMargins(0, 10, 0, 0);
    lunarLayout->addWidget(m_lunarLabel);
    lunarLayout->addWidget(m_lunarDetailLabel);
    m_lunarBox->setVisible(isChinese());

    auto *leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(0);
    leftLayout->setContentsMargins(20, 0, 10, 20);
    leftLayout->addStretch(1);
    leftLayout->addWidget(m_title);
    leftLayout->addStretch(2);
    leftLayout->addWidget(m_weekLabel);
    leftLayout->addWidget(m_dateLabel);
    leftLayout->addWidget(m_lunarBox);
    leftLayout->addSpacing(5);
    leftLayout->addWidget(m_jumpButton);

    auto *leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);
    leftWidget->setFixedWidth(230);

    m_headLabel->setFont(pixelFont(font(), 17, QFont::Medium));
    m_todayButton->setFixedSize(m_todayButton->sizeHint());
    m_previousButton->setFixedSize(m_previousButton->sizeHint());
    m_nextButton->setFixedSize(m_nextButton->sizeHint());

    auto *headLayout = new QHBoxLayout;
    headLayout->setContentsMargins(8, 0, 0, 0);
    headLayout->setSpacing(0);
    headLayout->addWidget(m_headLabel);
    headLayout->addStretch();
    headLayout->addWidget(m_todayButton);
    headLayout->addWidget(m_previousButton);
    headLayout->addWidget(m_nextButton);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(0);
    rightLayout->setContentsMargins(5, 5, 15, 15);
    rightLayout->addSpacing(6);
    rightLayout->addLayout(headLayout);
    rightLayout->addSpacing(6);
    rightLayout->addWidget(m_monthView, 1);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(leftWidget);
    mainLayout->addLayout(rightLayout, 1);

    m_dayTimer->setInterval(1000);

    connect(m_jumpButton, &V23CalendarButton::clicked, this, [this] {
        emit jumpButtonClicked();
    });
    connect(m_todayButton, &V23CalendarButton::clicked, this, &V23CalendarWidget::backToday);
    connect(m_previousButton, &V23CalendarButton::clicked, this, [this] { showMonthOffset(-1); });
    connect(m_nextButton, &V23CalendarButton::clicked, this, [this] { showMonthOffset(1); });
    connect(m_monthView, &V23MonthView::dateClicked, this, &V23CalendarWidget::setSelectedDate);
    connect(m_dayTimer, &QTimer::timeout, this, [this] {
        // past midnight while the popup is open
        if (QDate::currentDate() != m_today)
            backToday();
    });

    backToday();
}

void V23CalendarWidget::setSelectedDate(const QDate &date)
{
    if (!date.isValid())
        return;

    m_selected = date;
    m_displayed = QDate(date.year(), date.month(), 1);

    const QLocale locale = QLocale::system();
    m_title->setDate(date);
    m_weekLabel->setText(locale.dayName(date.dayOfWeek(), QLocale::LongFormat));
    m_dateLabel->setText(date.toString("yyyy/MM/dd"));
    m_headLabel->setText(monthName(date));

    m_todayButton->setVisible(m_displayed != QDate(m_today.year(), m_today.month(), 1));
    m_monthView->setMonth(m_displayed, m_selected);

    requestLunar(date);
}

void V23CalendarWidget::backToday()
{
    m_today = QDate::currentDate();
    setSelectedDate(m_today);
}

void V23CalendarWidget::showMonthOffset(int months)
{
    // like dde-tray-loader: the same day in the other month becomes selected
    setSelectedDate(m_selected.addMonths(months));
}

void V23CalendarWidget::requestLunar(const QDate &date)
{
    if (!m_lunarBox->isVisibleTo(this))
        return;

    // keep the old text until the reply arrives, so the panel does not jump
    m_lunarRequest = date;

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("com.deepin.api.LunarCalendar"),
        QStringLiteral("/com/deepin/api/LunarCalendar"),
        QStringLiteral("com.deepin.api.LunarCalendar"),
        QStringLiteral("GetLunarInfoBySolar"));
    message << date.year() << date.month() << date.day();

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    watcher->setProperty("date", date);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &V23CalendarWidget::onLunarReply);
}

void V23CalendarWidget::onLunarReply(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    // a later date was selected meanwhile
    if (watcher->property("date").toDate() != m_lunarRequest)
        return;

    const QDBusMessage reply = watcher->reply();
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2
            || !reply.arguments().at(1).toBool()) {
        qWarning() << "failed to get the lunar date:" << reply.errorMessage();
        m_lunarLabel->clear();
        m_lunarDetailLabel->clear();
        return;
    }

    // (GanZhiYear, GanZhiMonth, GanZhiDay, LunarMonthName, LunarDayName,
    //  LunarLeapMonth, Zodiac, Term, SolarFestival, LunarFestival, Worktime)
    QString ganZhiYear, ganZhiMonth, ganZhiDay, lunarMonth, lunarDay, zodiac;
    QString term, solarFestival, lunarFestival;
    int leapMonth = 0, worktime = 0;
    const QDBusArgument info = reply.arguments().at(0).value<QDBusArgument>();
    info.beginStructure();
    info >> ganZhiYear >> ganZhiMonth >> ganZhiDay >> lunarMonth >> lunarDay >> leapMonth
         >> zodiac >> term >> solarFestival >> lunarFestival >> worktime;
    info.endStructure();

    m_lunarLabel->setText(QStringLiteral("农历") + lunarMonth + lunarDay);
    setElidedText(m_lunarDetailLabel, ganZhiYear + QStringLiteral("年【") + zodiac + QStringLiteral("年】")
        + ganZhiMonth + QStringLiteral("月 ") + ganZhiDay + QStringLiteral("日"));
}

void V23CalendarWidget::setElidedText(QLabel *label, const QString &text)
{
    const int width = 230 - 20 - 10;
    const QString elided = label->fontMetrics().elidedText(text, Qt::ElideRight, width);
    label->setText(elided);
    label->setToolTip(elided == text ? QString() : text);
}

void V23CalendarWidget::updateColors()
{
    // the dock theme can change between two popups
    QPalette pa = palette();
    pa.setColor(QPalette::WindowText, textColor());
    for (QLabel *label : { m_weekLabel, m_dateLabel,
                           m_lunarLabel, m_lunarDetailLabel, m_headLabel })
        label->setPalette(pa);

    update();
}

void V23CalendarWidget::showEvent(QShowEvent *e)
{
    updateColors();
    backToday();
    m_dayTimer->start();

    QWidget::showEvent(e);
}

void V23CalendarWidget::hideEvent(QHideEvent *e)
{
    m_dayTimer->stop();

    QWidget::hideEvent(e);
}

void V23CalendarWidget::wheelEvent(QWheelEvent *e)
{
    m_deltaSum += e->angleDelta().y();
    if (m_deltaSum >= kTurnPageDelta) {
        showMonthOffset(-1);
        m_deltaSum = 0;
    } else if (m_deltaSum <= -kTurnPageDelta) {
        showMonthOffset(1);
        m_deltaSum = 0;
    }

    QWidget::wheelEvent(e);
}
