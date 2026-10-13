#include "informationwidget.h"
#include "constants.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QScreen>
#include <QApplication>
#include <QResizeEvent>

InformationWidget::InformationWidget(QWidget *parent)
    : QWidget(parent)
    , m_infoLabel(new QLabel)
    , m_baseFontPixelSize(12)
    , m_smallFontPixelSize(8)
    , m_smallSize(false)
{
    m_infoLabel->setStyleSheet("QLabel {"
                               "color: white;"
                               "}");
    //m_infoLabel->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);
    font.setFamily("Noto Mono");
    // 获取dpi，一般默认都是96，根据dpi进行字体的缩放，直接设置pointsize无法解决hidpi问题
    int dpi = QApplication::primaryScreen()->logicalDotsPerInch();
    int fontsize = 9; // >10在时尚模式下有些显示不全
    m_baseFontPixelSize = (dpi * fontsize) / 72;
    // 任务栏大小为"小"时条目高仅36px，两行大字(默认字号需46px)会超出任务栏，
    // 缩小到6pt(96dpi下8px，两行约28px)以保证完整显示
    int smallFontsize = 6;
    m_smallFontPixelSize = (dpi * smallFontsize) / 72;
    font.setPixelSize(m_baseFontPixelSize);
	m_infoLabel->setFont(font);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addWidget(m_infoLabel);
    centralLayout->setSpacing(5);
    centralLayout->setContentsMargins(5, 5, 5, 5);

    setLayout(centralLayout);
}

void InformationWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustForPanelSize();
}

void InformationWidget::adjustForPanelSize()
{
    // 高效/经典模式下任务栏条目尺寸由"大小"设置决定：
    // 上下方向条目高度 小(30)->36、中(36)->43、大(48)->58；
    // 左右方向条目宽度 小->50、中->60、大->80。
    // 条目尺寸小于下列阈值时视为"小"尺寸，需要缩小字体与边距。
    const Dock::Position position = qApp->property(PROP_POSITION).value<Dock::Position>();
    const bool horizontal = (position == Dock::Top || position == Dock::Bottom);
    const int extent = horizontal ? height() : width();
    const bool small = extent > 0 && extent <= (horizontal ? 40 : 55);

    if (small == m_smallSize) {
        return;
    }
    m_smallSize = small;

    font.setPixelSize(small ? m_smallFontPixelSize : m_baseFontPixelSize);
    m_infoLabel->setFont(font);

    // 同时压缩内边距，为文字腾出空间
    const int margin = small ? 2 : 5;
    if (QVBoxLayout *centralLayout = qobject_cast<QVBoxLayout *>(layout())) {
        centralLayout->setContentsMargins(margin, margin, margin, margin);
    }

    // 字号与边距变化后 sizeHint 随之改变，这里失效 Qt 的 sizeHint 缓存，
    // 否则面板拿到的仍是旧的条目尺寸(条目宽度会随字号变化)
    updateGeometry();
}

void InformationWidget::UpdateData(const Info& info,Dock::DisplayMode dismode,const Settings& settings)
{
    switch (settings.efficient) {
    case DisplayContentSetting::CPUMEM:
        m_infoLabel->setText(QString("<p style='line-height:%1%'>CPU:%2<br/>MEM:%3</p>")
                             .arg(settings.lineHeight)
                             .arg(info.cpu)
                             .arg(info.mem));
        break;
    case DisplayContentSetting::NETSPEED:
        m_infoLabel->setText(QString("<p style='line-height:%1%'>↑%2/S<br/>↓%3/S</p>")
                             .arg(settings.lineHeight)
                             .arg(info.netup)
                             .arg(info.netdwon));
        break;
    case DisplayContentSetting::ALL:
        m_infoLabel->setText(QString("<p style='line-height:%1%'>CPU:%2↑%3/S<br/>MEM:%4↓%5/S</p>")
                             .arg(settings.lineHeight)
                             .arg(info.cpu).arg(info.netup)
                             .arg(info.mem).arg(info.netdwon));
        break;
    case DisplayContentSetting::AllAndTempAndBattery:
        m_infoLabel->setText(QString("<p style='line-height:%1%'>CPU:%2↑%3/S Tem:%4°C<br/>MEM:%5↓%6/S Bat:%7W</p>")
                                 .arg(settings.lineHeight)
                                 .arg(info.cpu).arg(info.netup).arg(info.cpuTemp)
                                 .arg(info.mem).arg(info.netdwon).arg(info.battery));
        break;
    default:
        m_infoLabel->setText(QString("<p style='line-height:%1%'>CPU:%2↑%3/S<br/>MEM:%4↓%5/S</p>")
                             .arg(settings.lineHeight)
                             .arg(info.cpu).arg(info.netup)
                             .arg(info.mem).arg(info.netdwon));
        break;
    }
}
