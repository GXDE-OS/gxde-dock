#include "tipswidget.h"
#include "constants.h"

#include <QApplication>
#include <QFontInfo>
#include <QPainter>

TipsWidget::TipsWidget(QWidget *parent) : QFrame(parent)
{

}

void TipsWidget::setText(const QString &text)
{
    m_text = text;

    if (Dock::isV23Mode()) {
        QFont f = QApplication::font();
        f.setPixelSize(qMax(12, QFontInfo(f).pixelSize() - 2));
        setFont(f);
        m_v23Font = true;
        setFixedSize(fontMetrics().horizontalAdvance(text) + 24, fontMetrics().height() + 12);
    } else {
        if (m_v23Font) {
            setFont(QFont());
            m_v23Font = false;
        }
        setFixedSize(fontMetrics().horizontalAdvance(text) + 6, fontMetrics().height());
    }

    update();
}

void TipsWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);

    QPen pen(Dock::isV23Light() ? Qt::black : Qt::white);
    painter.setPen(pen);

    QTextOption option;
    option.setAlignment(Qt::AlignCenter);
    painter.drawText(rect(), m_text, option);
}
