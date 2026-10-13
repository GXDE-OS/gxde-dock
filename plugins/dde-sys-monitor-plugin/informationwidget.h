#ifndef INFORMATIONWIDGET_H
#define INFORMATIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QObject>
#include "pluginsiteminterface.h"
#include "pluginsettingdialog.h"

struct Info{
    QString cpu;
    QString mem;
    QString netup;
    QString netdwon;
    QString battery;
    QString cpuTemp;
};

class InformationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InformationWidget(QWidget *parent = nullptr);
    void UpdateData(const Info& info,Dock::DisplayMode dismode,const Settings& settings);

protected:
    // 条目尺寸变化时(切换任务栏大小/位置/显示模式)调整字体与边距
    void resizeEvent(QResizeEvent *event) override;

public:
    // 真正的数据显示在这个 Label 上
    QLabel *m_infoLabel;
    // 字体
	QFont font;

private:
    // 任务栏尺寸为"小"时条目容纳不下两行大字，需要缩小字体与边距
    void adjustForPanelSize();

    // 基准字号(像素)与小尺寸下的字号(像素)
    int m_baseFontPixelSize;
    int m_smallFontPixelSize;
    // 当前是否处于小尺寸模式
    bool m_smallSize;
};

#endif // INFORMATIONWIDGET_H
