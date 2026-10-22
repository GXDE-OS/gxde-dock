#ifndef TIPSWIDGET_H
#define TIPSWIDGET_H

#include <QFrame>

class TipsWidget : public QFrame
{
    Q_OBJECT
public:
    explicit TipsWidget(QWidget *parent = nullptr);

    void setText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    bool m_v23Font = false;
};

#endif // TIPSWIDGET_H
