/*
 * Copyright (C) 2026 GXDE OS
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
 */

#include "bluetoothapplet.h"
#include "bluetoothicon.h"

#include <DDBusSender>

#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QAbstractButton>
#include <QSizePolicy>
#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QShowEvent>

#include <algorithm>

namespace {

constexpr int Radius = 8;
constexpr int ListItemHeight = 36;
constexpr int PanelSpacing = 10;
constexpr int PanelWidth = 330;
constexpr int MaxContentHeight = 480;
constexpr int MinContentHeight = 340;

const QColor Text = Qt::white;                 // 深色 dock/弹窗上的文字色
const QColor Highlight(0x02, 0x4c, 0xca);      // 强调色（V23 深色主题）

QColor withAlpha(const QColor &color, qreal alpha)
{
    QColor c = color;
    c.setAlphaF(alpha);
    return c;
}

QFont pixelFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font = qApp->font();
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

void drawIcon(QPainter &painter, const QRect &rect, const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return;
    const QSizeF size = pixmap.deviceIndependentSize();
    const QPointF topLeft(rect.x() + (rect.width() - size.width()) / 2.0,
                          rect.y() + (rect.height() - size.height()) / 2.0);
    painter.drawPixmap(topLeft, pixmap);
}

QString deviceIcon(const QJsonObject &device)
{
    static const QList<QPair<QString, QString>> icons {
        { "audio-headset", "bluetooth_pheadset" },
        { "audio-headphones", "bluetooth_pheadset" },
        { "audio-card", "bluetooth_clang" },
        { "audio", "bluetooth_clang" },
        { "camera-video", "bluetooth_vidicon" },
        { "camera", "bluetooth_camera" },
        { "input-keyboard", "bluetooth_keyboard" },
        { "input-mouse", "bluetooth_mouse" },
        { "input-tablet", "bluetooth_pad" },
        { "input-gaming", "bluetooth_other" },
        { "phone", "bluetooth_phone" },
        { "computer", "bluetooth_pc" },
        { "printer", "bluetooth_print" },
        { "scanner", "bluetooth_scaner" },
        { "video-display", "bluetooth_tv" },
        { "network", "bluetooth_lan" },
    };
    const QString icon = device.value("Icon").toString();
    for (const auto &pair : icons) {
        if (icon.startsWith(pair.first))
            return pair.second;
    }
    return QStringLiteral("bluetooth_other");
}

QString deviceName(const QJsonObject &device)
{
    const QString alias = device.value("Alias").toString();
    return alias.isEmpty() ? device.value("Name").toString() : alias;
}

// 一个胶囊开关，样式参考 V23Switch
class BTSwitch : public QAbstractButton
{
public:
    explicit BTSwitch(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setCheckable(true);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(40, 22);
    }

    QSize sizeHint() const override { return QSize(40, 22); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);

        const QRectF track = rect();
        p.setBrush(isChecked() ? Highlight : withAlpha(Text, 0.2));
        p.drawRoundedRect(track, track.height() / 2, track.height() / 2);

        const qreal knob = track.height() - 4;
        const qreal x = isChecked() ? track.right() - 2 - knob : track.left() + 2;
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(x, 2, knob, knob));
    }
};

// 节标题：文本，右侧可放一个开关，样式参考 V23SectionHeader
class BTSectionHeader : public QWidget
{
public:
    BTSectionHeader(const QString &title, bool withSwitch, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_title(title)
    {
        setFixedHeight(withSwitch ? ListItemHeight : 24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        if (withSwitch) {
            m_switch = new BTSwitch(this);
            QHBoxLayout *layout = new QHBoxLayout(this);
            layout->setContentsMargins(10, 0, 10, 0);
            layout->addStretch();
            layout->addWidget(m_switch);
        }
    }

    void setChecked(bool checked)
    {
        if (m_switch)
            m_switch->setChecked(checked);
    }

    BTSwitch *toggle() const { return m_switch; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setFont(pixelFont(11, QFont::Medium));
        p.setPen(withAlpha(Text, 0.7));
        const int right = m_switch ? m_switch->geometry().left() - 10 : width() - 10;
        p.drawText(QRect(10, 0, right - 10, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(p.font()).elidedText(m_title, Qt::ElideRight, right - 10));
    }

private:
    QString m_title;
    BTSwitch *m_switch = nullptr;
};

// 列表项：图标 + 名称 + 右侧状态（已连接勾 / 箭头 / 状态文字），
// 样式参考 V23ListItem（悬停高亮）
class BTListItem : public QAbstractButton
{
public:
    explicit BTListItem(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setCheckable(false);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(ListItemHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }

    void setIconName(const QString &name) { m_iconName = name; update(); }
    void setSelected(bool selected) { m_selected = selected; update(); }
    void setArrow(bool arrow) { m_arrow = arrow; update(); }
    void setStatusText(const QString &text) { m_status = text; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool highlighted = (underMouse() || isDown()) && isEnabled();
        const QColor fg = highlighted ? QColor(Qt::white) : Text;
        const qreal ratio = devicePixelRatioF();

        p.setPen(Qt::NoPen);
        p.setBrush(highlighted ? Highlight : withAlpha(Text, 0.05));
        p.drawRoundedRect(rect(), Radius, Radius);

        int left = 10;
        if (!m_iconName.isEmpty()) {
            drawIcon(p, QRect(left, (height() - 16) / 2, 16, 16),
                     bluetoothIcon(m_iconName, 16, ratio));
            left += 16 + 10;
        }

        int right = width() - 10;
        if (m_selected) {
            drawIcon(p, QRect(right - 16, (height() - 16) / 2, 16, 16),
                     bluetoothIcon(QStringLiteral("plugin_item_select_16"), 16, ratio));
            right -= 16 + 6;
        } else if (m_arrow) {
            drawIcon(p, QRect(right - 16, (height() - 16) / 2, 16, 16),
                     bluetoothIcon(QStringLiteral("open-arrow_16"), 16, ratio));
            right -= 16 + 6;
        }

        if (!m_status.isEmpty()) {
            const QFont statusFont = pixelFont(12);
            const QFontMetrics metrics(statusFont);
            const int statusWidth = qMin(metrics.horizontalAdvance(m_status), (right - left) / 2);
            p.setPen(withAlpha(fg, 0.6));
            p.setFont(statusFont);
            p.drawText(QRect(right - statusWidth, 0, statusWidth, height()),
                       Qt::AlignRight | Qt::AlignVCenter,
                       metrics.elidedText(m_status, Qt::ElideRight, statusWidth));
            right -= statusWidth + 6;
        }

        p.setPen(isEnabled() ? fg : withAlpha(fg, 0.4));
        p.setFont(pixelFont(14));
        p.drawText(QRect(left, 0, right - left, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(p.font()).elidedText(text(), Qt::ElideRight, right - left));
    }

private:
    QString m_iconName;
    QString m_status;
    bool m_selected = false;
    bool m_arrow = false;
};

void addSectionLabel(QVBoxLayout *layout, const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setStyleSheet(QStringLiteral("color:rgba(255,255,255,0.6);background:transparent;"));
    label->setContentsMargins(10, 6, 10, 2);
    layout->addWidget(label);
}

}

BluetoothApplet::BluetoothApplet(DBusBluetooth *inter, QWidget *parent)
    : QWidget(parent)
    , m_bluetoothInter(inter)
{
    setFixedWidth(PanelWidth);
    setAttribute(Qt::WA_TranslucentBackground);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));
    m_scroll->setMaximumHeight(MaxContentHeight);

    m_content = new QWidget(m_scroll);
    m_content->setStyleSheet(QStringLiteral("background:transparent;"));
    m_layout = new QVBoxLayout(m_content);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(PanelSpacing);
    m_scroll->setWidget(m_content);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(PanelSpacing, PanelSpacing, PanelSpacing, PanelSpacing);
    root->addWidget(m_scroll);
    root->addStretch();

    connect(m_bluetoothInter, &DBusBluetooth::adaptersChanged, this, [this] { rebuild(); });
    connect(m_bluetoothInter, &DBusBluetooth::devicesChanged, this, [this] { rebuild(); });

    rebuild();
}

QSize BluetoothApplet::sizeHint() const
{
    const int content = m_content ? m_content->sizeHint().height() : 0;
    const int height = qMax(MinContentHeight, qMin(content, MaxContentHeight));
    return QSize(PanelWidth, height + 2 * PanelSpacing);
}

void BluetoothApplet::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    for (const QJsonObject &adapter : m_bluetoothInter->adapters()) {
        if (adapter.value("Powered").toBool())
            m_bluetoothInter->requestDiscovery(adapter.value("Path").toString());
    }
}

void BluetoothApplet::openSettings()
{
    DDBusSender()
        .service("com.deepin.dde.ControlCenter")
        .interface("com.deepin.dde.ControlCenter")
        .path("/com/deepin/dde/ControlCenter")
        .method(QStringLiteral("ShowModule"))
        .arg(QStringLiteral("bluetooth"))
        .call();
}

void BluetoothApplet::rebuild()
{
    if (!m_layout)
        return;

    QLayoutItem *child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (const QJsonObject &adapter : m_bluetoothInter->adapters()) {
        const QString adapterPath = adapter.value("Path").toString();
        const bool powered = adapter.value("Powered").toBool();
        QString alias = adapter.value("Alias").toString();
        if (alias.isEmpty())
            alias = tr("Bluetooth");

        BTSectionHeader *header = new BTSectionHeader(alias, true, m_content);
        header->setChecked(powered);
        connect(header->toggle(), &QAbstractButton::toggled, this,
                [this, adapterPath](bool on) { m_bluetoothInter->setPowered(on); });
        m_layout->addWidget(header);

        if (!powered)
            continue;

        for (bool paired : { true, false }) {
            QList<QJsonObject> devices;
            for (const QJsonObject &device : m_bluetoothInter->devices(adapterPath)) {
                if (device.value("Paired").toBool() == paired
                        && (paired || !deviceName(device).isEmpty()))
                    devices << device;
            }
            if (devices.isEmpty())
                continue;

            std::stable_sort(devices.begin(), devices.end(),
                             [](const QJsonObject &a, const QJsonObject &b) {
                return a.value("State").toInt() > b.value("State").toInt();
            });

            addSectionLabel(m_layout, paired ? tr("My Devices") : tr("Other Devices"));

            for (const QJsonObject &device : devices) {
                const QString path = device.value("Path").toString();
                const int state = device.value("State").toInt();
                const bool connected = state == BluetoothConnected;

                BTListItem *item = new BTListItem(m_content);
                item->setIconName(deviceIcon(device) + QStringLiteral("_16"));
                item->setText(deviceName(device));
                item->setSelected(connected);
                if (state == BluetoothConnecting)
                    item->setStatusText(tr("Connecting…"));
                connect(item, &QAbstractButton::clicked, this, [this, path, connected] {
                    if (connected)
                        m_bluetoothInter->disconnectDevice(path);
                    else
                        m_bluetoothInter->connectDevice(path);
                });
                m_layout->addWidget(item);
            }
        }
    }

    BTListItem *settings = new BTListItem(m_content);
    settings->setText(tr("Bluetooth settings"));
    settings->setArrow(true);
    connect(settings, &QAbstractButton::clicked, this, [this] { openSettings(); });
    m_layout->addWidget(settings);

    updateGeometry();
}
