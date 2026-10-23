// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// The looks follow the quick panel of Deepin V23: PluginItem.qml and
// SubPluginPage.qml of dde-shell for the cards and pages, and the common
// widgets of dde-tray-loader (SignalQuickPanel, SliderContainer,
// PluginItemDelegate, JumpSettingButton) for what is on them.

#include "v23quickwidgets.h"
#include "constants.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

using namespace V23Quick;

namespace {
constexpr int Radius = 8;
constexpr int ListItemHeight = 36;
constexpr int HandleSize = 20;

void drawIcon(QPainter &painter, const QRect &rect, const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return;

    const QSizeF size = pixmap.deviceIndependentSize();
    const QPointF topLeft(rect.x() + (rect.width() - size.width()) / 2.0,
                          rect.y() + (rect.height() - size.height()) / 2.0);
    painter.drawPixmap(topLeft, pixmap);
}

// the faint background of the entries of a page
QColor itemBackground()
{
    return withAlpha(popupTextColor(), 0.05);
}

bool horizontalDock()
{
    const auto position = qApp->property(PROP_POSITION).value<Dock::Position>();
    return position == Dock::Top || position == Dock::Bottom;
}
}

QPixmap V23Icon::pixmap(const QColor &color, qreal devicePixelRatio) const
{
    if (isNull())
        return QPixmap();

    return V23Quick::icon(name, size, color, devicePixelRatio, mode);
}

V23DockIcon::V23DockIcon(QWidget *parent)
    : QWidget(parent)
{
}

void V23DockIcon::setIcons(const QList<V23Icon> &icons)
{
    if (icons == m_icons)
        return;

    const bool resized = icons.size() != m_icons.size();
    m_icons = icons;
    if (resized)
        updateGeometry();
    update();
}

QSize V23DockIcon::sizeHint() const
{
    // a 16px icon every 22px, like the tray of dde-shell
    const int length = 4 + 22 * qMax(1, int(m_icons.size()));
    return horizontalDock() ? QSize(qMax(26, length), 26) : QSize(26, qMax(26, length));
}

void V23DockIcon::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    const bool horizontal = horizontalDock();
    const int length = 22 * m_icons.size();
    int start = ((horizontal ? width() : height()) - length) / 2;
    for (const V23Icon &icon : m_icons) {
        const QRect slot = horizontal ? QRect(start, 0, 22, height()) : QRect(0, start, width(), 22);
        drawIcon(painter, slot, icon.pixmap(dockTextColor(), devicePixelRatioF()));
        start += 22;
    }
}

void V23DockIcon::wheelEvent(QWheelEvent *e)
{
    emit wheelScrolled(e->angleDelta().y());
    e->accept();
}

V23QuickCard::V23QuickCard(QWidget *parent)
    : QWidget(parent)
    , m_active(false)
    , m_hover(false)
    , m_pressed(false)
{
}

void V23QuickCard::setActive(bool active)
{
    m_active = active;
    update();
}

void V23QuickCard::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    // the "crystal" colors of PluginItem.qml
    const bool light = lightPopup();
    qreal alpha;
    if (m_active)
        alpha = light ? (m_pressed ? 0.95 : (m_hover ? 0.9 : 0.8)) : (m_pressed ? 0.85 : (m_hover ? 0.8 : 0.7));
    else
        alpha = light ? (m_pressed ? 0.45 : (m_hover ? 0.4 : 0.3)) : (m_pressed ? 0.15 : (m_hover ? 0.1 : 0.05));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(withAlpha(Qt::black, light ? 0.03 : 0.05));
    painter.setBrush(withAlpha(Qt::white, alpha));
    painter.drawRoundedRect(outer, Radius, Radius);
    painter.setPen(withAlpha(Qt::white, light ? 0.1 : 0.05));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outer.adjusted(1, 1, -1, -1), Radius - 1, Radius - 1);
}

void V23QuickCard::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QWidget::enterEvent(e);
}

void V23QuickCard::leaveEvent(QEvent *e)
{
    m_hover = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(e);
}

void V23QuickCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(e);
}

void V23QuickCard::mouseReleaseEvent(QMouseEvent *e)
{
    const bool click = m_pressed && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint());
    m_pressed = false;
    update();
    if (click)
        emit clicked();
    QWidget::mouseReleaseEvent(e);
}

V23CircleButton::V23CircleButton(Style style, int diameter, const QSize &iconSize, QWidget *parent)
    : QAbstractButton(parent)
    , m_style(style)
    , m_diameter(diameter)
    , m_active(false)
    , m_hover(false)
{
    m_icon.size = iconSize;
    setFixedSize(diameter, diameter);
    setFocusPolicy(Qt::NoFocus);
}

void V23CircleButton::setStyle(Style style)
{
    m_style = style;
    update();
}

void V23CircleButton::setIcon(const QString &name, IconMode mode)
{
    m_icon.name = name;
    m_icon.mode = mode;
    update();
}

void V23CircleButton::setActive(bool active)
{
    m_active = active;
    update();
}

QSize V23CircleButton::sizeHint() const
{
    return QSize(m_diameter, m_diameter);
}

void V23CircleButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const bool light = lightPopup();
    QColor background = Qt::transparent;
    QColor foreground = popupTextColor();
    switch (m_style) {
    case Switch:
        if (m_active) {
            // QuickButton of dde-tray-loader
            background = highlightColor();
            if (isDown())
                background.setHslF(background.hslHueF(), background.hslSaturationF(), background.lightnessF() * 0.9);
            else if (m_hover)
                background.setHslF(background.hslHueF(), background.hslSaturationF(), qMin(1.0, background.lightnessF() * 1.1));
            foreground = withAlpha(Qt::white, isDown() ? 0.8 : 1.0);
        } else {
            background = withAlpha(popupTextColor(), isDown() ? 0.2 : (m_hover ? 0.15 : 0.1));
        }
        break;
    case Plain:
        // RightIconButton of dde-tray-loader
        background = light ? withAlpha(Qt::black, isDown() ? 0.2 : (m_hover ? 0.15 : 0.1))
                           : withAlpha(Qt::white, isDown() ? 0.25 : (m_hover ? 0.2 : 0.1));
        break;
    case Flat:
        if (isDown() || m_hover)
            background = light ? withAlpha(Qt::black, isDown() ? 0.15 : 0.1) : withAlpha(Qt::white, isDown() ? 0.05 : 0.1);
        break;
    case Display:
        break;
    }

    painter.setBrush(background);
    if (m_style == Flat)
        painter.drawRoundedRect(rect(), Radius, Radius);
    else
        painter.drawEllipse(rect());
    drawIcon(painter, rect(), m_icon.pixmap(foreground, devicePixelRatioF()));
}

void V23CircleButton::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(e);
}

void V23CircleButton::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(e);
}

V23QuickTile::V23QuickTile(Style style, QWidget *parent)
    : V23QuickCard(parent)
    , m_style(style)
    , m_button(nullptr)
    , m_iconActive(false)
{
    if (style == Large) {
        setFixedSize(LargeTileWidth, TileHeight);
        m_button = new V23CircleButton(V23CircleButton::Switch, 40, QSize(22, 22), this);
        m_button->move(10, (TileHeight - m_button->height()) / 2);
        connect(m_button, &V23CircleButton::clicked, this, &V23QuickTile::iconClicked);
    } else {
        setFixedSize(TileWidth, TileHeight);
        m_icon.size = QSize(24, 24);
    }
}

void V23QuickTile::setIcon(const QString &name, IconMode mode)
{
    if (m_button) {
        m_button->setIcon(name, mode);
    } else {
        m_icon.name = name;
        m_icon.mode = mode;
        update();
    }
}

void V23QuickTile::setIconActive(bool active)
{
    m_iconActive = active;
    if (m_button)
        m_button->setActive(active);
    update();
}

void V23QuickTile::setIconDisplayOnly(bool displayOnly)
{
    if (m_button)
        m_button->setStyle(displayOnly ? V23CircleButton::Display : V23CircleButton::Switch);
}

void V23QuickTile::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void V23QuickTile::setDescription(const QString &description)
{
    m_description = description;
    update();
}

void V23QuickTile::paintEvent(QPaintEvent *e)
{
    V23QuickCard::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor text = popupTextColor();

    if (m_style == Standard) {
        // SignalQuickPanel: a 24px icon, 10px apart from its name
        const QColor color = m_iconActive ? highlightColor() : text;
        const QFont font = pixelFont(10);
        const QFontMetrics metrics(font);
        const int contentHeight = 24 + 10 + metrics.height();
        const int top = (height() - contentHeight) / 2;
        drawIcon(painter, QRect(0, top, width(), 24), m_icon.pixmap(color, devicePixelRatioF()));
        painter.setPen(color);
        painter.setFont(font);
        const QRect textRect(8, top + 34, width() - 16, metrics.height());
        painter.drawText(textRect, Qt::AlignCenter, metrics.elidedText(m_title, Qt::ElideRight, textRect.width()));
        return;
    }

    // QuickPanelWidget: the name and the state between the icon and the arrow
    const int left = m_button->geometry().right() + 1 + 10;
    const int arrowSize = 16;
    const int arrowRight = width() - 10;
    const int right = arrowRight - arrowSize - 4;
    const QFont titleFont = pixelFont(14, QFont::Medium);
    const QFont descriptionFont = pixelFont(10);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descriptionMetrics(descriptionFont);
    const int textHeight = titleMetrics.height() + descriptionMetrics.height();
    const int top = (height() - textHeight) / 2;

    painter.setPen(text);
    painter.setFont(titleFont);
    painter.drawText(QRect(left, top, right - left, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(m_title, Qt::ElideRight, right - left));
    painter.setPen(withAlpha(text, 0.7));
    painter.setFont(descriptionFont);
    painter.drawText(QRect(left, top + titleMetrics.height(), right - left, descriptionMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     descriptionMetrics.elidedText(m_description, Qt::ElideRight, right - left));

    const QRect arrowRect(arrowRight - arrowSize, (height() - arrowSize) / 2, arrowSize, arrowSize);
    drawIcon(painter, arrowRect, V23Quick::icon(QStringLiteral("arrow-right"), QSize(arrowSize, arrowSize),
                                                text, devicePixelRatioF()));
}

V23Slider::V23Slider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
{
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(HandleSize + 4);
    setPageStep(2);
}

QSize V23Slider::sizeHint() const
{
    return QSize(160, HandleSize + 4);
}

QRect V23Slider::grooveRect() const
{
    const int margin = HandleSize / 2;
    return QRect(margin, (height() - 4) / 2, width() - 2 * margin, 4);
}

int V23Slider::valueAt(int x) const
{
    const QRect groove = grooveRect();
    return QStyle::sliderValueFromPosition(minimum(), maximum(), x - groove.left(), groove.width());
}

void V23Slider::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    // SliderProxyStyle::drawRoundSlider: black on light, white on dark, the
    // rest of the line at 20%
    QColor color = isEnabled() ? (lightPopup() ? QColor(Qt::black) : QColor(Qt::white)) : QColor(Qt::gray);
    const QRect groove = grooveRect();
    const qreal radius = groove.height() / 2.0;

    painter.setBrush(withAlpha(color, 0.2));
    painter.drawRoundedRect(groove, radius, radius);

    const int position = QStyle::sliderPositionFromValue(minimum(), maximum(), value(), groove.width());
    QRect filled = groove;
    filled.setWidth(position);
    painter.setBrush(color);
    painter.drawRoundedRect(filled, radius, radius);
    painter.drawEllipse(QRect(groove.left() + position - HandleSize / 2, (height() - HandleSize) / 2,
                              HandleSize, HandleSize));
}

void V23Slider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return QSlider::mousePressEvent(e);

    setSliderDown(true);
    setValue(valueAt(e->position().toPoint().x()));
    e->accept();
}

void V23Slider::mouseMoveEvent(QMouseEvent *e)
{
    if (!isSliderDown())
        return QSlider::mouseMoveEvent(e);

    setValue(valueAt(e->position().toPoint().x()));
    e->accept();
}

void V23Slider::mouseReleaseEvent(QMouseEvent *e)
{
    if (!isSliderDown())
        return QSlider::mouseReleaseEvent(e);

    setSliderDown(false);
    e->accept();
}

void V23Slider::wheelEvent(QWheelEvent *e)
{
    const int delta = e->angleDelta().y();
    if (delta != 0)
        setValue(value() + (delta > 0 ? pageStep() : -pageStep()));
    e->accept();
}

V23SliderRow::V23SliderRow(Style style, QWidget *parent)
    : V23QuickCard(parent)
    , m_style(style)
    , m_leftButton(new V23CircleButton(V23CircleButton::Display, style == Panel ? 24 : 16,
                                       style == Panel ? QSize(24, 24) : QSize(16, 16), this))
    , m_slider(new V23Slider(this))
    , m_rightButton(style == Panel ? new V23CircleButton(V23CircleButton::Plain, 36, QSize(24, 24), this)
                                   : new V23CircleButton(V23CircleButton::Display, 16, QSize(16, 16), this))
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    if (style == Panel) {
        // BrightnessQuickPanel / SoundQuickPanel
        setFixedSize(FullTileWidth, TileHeight);
        layout->setContentsMargins(14, 0, 12, 0);
    } else {
        // the volume bar of SoundApplet
        setFixedSize(PageWidth, ListItemHeight);
        layout->setContentsMargins(10, 0, 10, 0);
    }
    layout->addWidget(m_leftButton);
    layout->addSpacing(10);
    layout->addWidget(m_slider, 1);
    layout->addSpacing(10);
    layout->addWidget(m_rightButton);

    m_leftButton->setEnabled(false);
    m_rightButton->setVisible(false);

    connect(m_slider, &QSlider::valueChanged, this, &V23SliderRow::valueChanged);
    connect(m_leftButton, &V23CircleButton::clicked, this, &V23SliderRow::leftClicked);
    connect(m_rightButton, &V23CircleButton::clicked, this, &V23SliderRow::rightClicked);
}

void V23SliderRow::setLeftIcon(const QString &name)
{
    m_leftButton->setIcon(name);
}

void V23SliderRow::setLeftClickable(bool clickable)
{
    m_leftButton->setEnabled(clickable);
}

void V23SliderRow::setRightIcon(const QString &name)
{
    m_rightButton->setIcon(name);
    m_rightButton->setVisible(!name.isEmpty());
}

void V23SliderRow::setRange(int minimum, int maximum)
{
    QSignalBlocker blocker(m_slider);
    m_slider->setRange(minimum, maximum);
}

void V23SliderRow::setValue(int value)
{
    // a value coming back from the service must not fight with the dragging
    if (m_slider->isSliderDown())
        return;

    QSignalBlocker blocker(m_slider);
    m_slider->setValue(value);
    m_slider->update();
}

int V23SliderRow::value() const
{
    return m_slider->value();
}

void V23SliderRow::paintEvent(QPaintEvent *e)
{
    if (m_style == Panel) {
        V23QuickCard::paintEvent(e);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(itemBackground());
    painter.drawRoundedRect(rect(), Radius, Radius);
}

V23Switch::V23Switch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(sizeHint());
}

QSize V23Switch::sizeHint() const
{
    return QSize(40, 22);
}

void V23Switch::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const QRectF track = rect();
    painter.setBrush(isChecked() ? highlightColor() : withAlpha(popupTextColor(), 0.2));
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);

    const qreal knob = track.height() - 4;
    const qreal x = isChecked() ? track.right() - 2 - knob : track.left() + 2;
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(x, 2, knob, knob));
}

V23SectionHeader::V23SectionHeader(const QString &title, bool withSwitch, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_switch(nullptr)
{
    setFixedHeight(withSwitch ? 36 : 24);
    if (withSwitch) {
        m_switch = new V23Switch(this);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 0, 10, 0);
        layout->addStretch();
        layout->addWidget(m_switch);
        connect(m_switch, &V23Switch::clicked, this, &V23SectionHeader::toggled);
    }
}

void V23SectionHeader::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void V23SectionHeader::setRightText(const QString &text)
{
    m_rightText = text;
    update();
}

void V23SectionHeader::setChecked(bool checked)
{
    if (m_switch)
        m_switch->setChecked(checked);
}

QSize V23SectionHeader::sizeHint() const
{
    return QSize(PageWidth, height());
}

void V23SectionHeader::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    // the T9 medium titles of the dde-tray-loader applets
    QPainter painter(this);
    const QFont font = pixelFont(11, QFont::Medium);
    const QFontMetrics metrics(font);
    int right = m_switch ? m_switch->geometry().left() - 10 : width() - 10;
    painter.setFont(font);
    if (!m_rightText.isEmpty()) {
        painter.setPen(popupTextColor());
        painter.drawText(QRect(10, 0, right - 10, height()), Qt::AlignRight | Qt::AlignVCenter, m_rightText);
        right -= metrics.horizontalAdvance(m_rightText) + 10;
    }
    painter.setPen(withAlpha(popupTextColor(), 0.7));
    painter.drawText(QRect(10, 0, right - 10, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(m_title, Qt::ElideRight, right - 10));
}

V23ListItem::V23ListItem(QWidget *parent)
    : QAbstractButton(parent)
    , m_selected(false)
    , m_busy(false)
    , m_arrow(false)
    , m_hover(false)
    , m_busyAngle(0)
    , m_busyTimer(nullptr)
{
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(ListItemHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void V23ListItem::setIcon(const V23Icon &icon)
{
    m_icon = icon;
    update();
}

void V23ListItem::setStatusText(const QString &text)
{
    m_statusText = text;
    update();
}

void V23ListItem::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void V23ListItem::setBusy(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    if (busy && !m_busyTimer) {
        m_busyTimer = new QTimer(this);
        m_busyTimer->setInterval(50);
        connect(m_busyTimer, &QTimer::timeout, this, [this] {
            m_busyAngle = (m_busyAngle + 20) % 360;
            update();
        });
    }
    if (m_busyTimer) {
        if (busy)
            m_busyTimer->start();
        else
            m_busyTimer->stop();
    }
    update();
}

void V23ListItem::setArrow(bool arrow)
{
    m_arrow = arrow;
    update();
}

void V23ListItem::setHoverAction(const QString &text)
{
    m_hoverAction = text;
    update();
}

QSize V23ListItem::sizeHint() const
{
    return QSize(PageWidth, ListItemHeight);
}

QRect V23ListItem::actionRect() const
{
    const QFontMetrics metrics(pixelFont(12));
    const int width = metrics.horizontalAdvance(m_hoverAction) + 16;
    return QRect(this->width() - 10 - width, (height() - 24) / 2, width, 24);
}

void V23ListItem::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    // PluginItemDelegate and JumpSettingButton: a faint card, the highlight
    // color with white text under the mouse
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool highlighted = (m_hover || isDown()) && isEnabled();
    const QColor text = highlighted ? QColor(Qt::white) : popupTextColor();
    const qreal ratio = devicePixelRatioF();

    painter.setPen(Qt::NoPen);
    painter.setBrush(highlighted ? highlightColor() : itemBackground());
    painter.drawRoundedRect(rect(), Radius, Radius);

    int left = 10;
    if (!m_icon.isNull()) {
        const QRect iconRect(left, (height() - 16) / 2, 16, 16);
        drawIcon(painter, iconRect, m_icon.pixmap(isEnabled() ? text : withAlpha(text, 0.4), ratio));
        left = iconRect.right() + 1 + 10;
    }

    int right = width() - 10;
    auto takeRight = [&right, this](int size) {
        const QRect r(right - size, (height() - size) / 2, size, size);
        right = r.left() - 6;
        return r;
    };

    if (m_selected && m_hover && !m_hoverAction.isEmpty()) {
        // the "Disconnect" text button
        const QRect r = actionRect();
        painter.setBrush(withAlpha(Qt::white, 0.2));
        painter.drawRoundedRect(r, 6, 6);
        painter.setPen(text);
        painter.setFont(pixelFont(12));
        painter.drawText(r, Qt::AlignCenter, m_hoverAction);
        right = r.left() - 6;
    } else if (m_busy) {
        const QRect r = takeRight(16);
        painter.save();
        painter.translate(QRectF(r).center());
        painter.rotate(m_busyAngle);
        painter.translate(-QRectF(r).center());
        drawIcon(painter, r, V23Quick::icon(QStringLiteral("refresh"), QSize(16, 16), text, ratio));
        painter.restore();
    } else if (m_selected) {
        drawIcon(painter, takeRight(16), V23Quick::icon(QStringLiteral("plugin_item_select"), QSize(16, 16), text, ratio));
    } else if (m_arrow) {
        drawIcon(painter, takeRight(16), V23Quick::icon(QStringLiteral("open-arrow"), QSize(16, 16), text, ratio));
    }

    if (!m_statusText.isEmpty()) {
        const QFont statusFont = pixelFont(12);
        const QFontMetrics metrics(statusFont);
        const int statusWidth = qMin(metrics.horizontalAdvance(m_statusText), (right - left) / 2);
        painter.setPen(withAlpha(text, 0.6));
        painter.setFont(statusFont);
        painter.drawText(QRect(right - statusWidth, 0, statusWidth, height()), Qt::AlignRight | Qt::AlignVCenter,
                         metrics.elidedText(m_statusText, Qt::ElideRight, statusWidth));
        right -= statusWidth + 6;
    }

    const QFont font = pixelFont(14);
    painter.setPen(isEnabled() ? text : withAlpha(text, 0.4));
    painter.setFont(font);
    painter.drawText(QRect(left, 0, right - left, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(font).elidedText(this->text(), Qt::ElideRight, right - left));
}

void V23ListItem::enterEvent(QEnterEvent *e)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(e);
}

void V23ListItem::leaveEvent(QEvent *e)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(e);
}

void V23ListItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && isDown() && m_selected && !m_hoverAction.isEmpty()
            && actionRect().contains(e->position().toPoint())) {
        setDown(false);
        update();
        emit actionClicked();
        return;
    }

    QAbstractButton::mouseReleaseEvent(e);
}

V23ListView::V23ListView(int maximumHeight, QWidget *parent)
    : QScrollArea(parent)
    , m_maximumHeight(maximumHeight)
    , m_content(new QWidget)
    , m_layout(new QVBoxLayout(m_content))
{
    // the entries of PluginItemDelegate keep 10px apart
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(PanelSpacing);

    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);
    setWidget(m_content);
    viewport()->setAutoFillBackground(false);
    m_content->setAutoFillBackground(false);
    setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));

    m_content->installEventFilter(this);
    setFixedHeight(0);
}

void V23ListView::clear()
{
    while (QLayoutItem *item = m_layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

bool V23ListView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_content && event->type() == QEvent::LayoutRequest)
        QMetaObject::invokeMethod(this, &V23ListView::updateHeight, Qt::QueuedConnection);

    return QScrollArea::eventFilter(watched, event);
}

void V23ListView::updateHeight()
{
    setFixedHeight(qMin(m_layout->sizeHint().height(), m_maximumHeight));
}

V23DetailPage::V23DetailPage(const QString &title, QWidget *content, bool inPanel, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_content(content)
    , m_backButton(nullptr)
    , m_headerHeight(inPanel ? 10 + 20 : 0)
    , m_minimumHeight(0)
{
    setFixedWidth(PanelWidth);

    // the pages of the applets keep 10px off the edges
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(PanelSpacing, m_headerHeight + PanelSpacing, PanelSpacing, PanelSpacing);
    layout->setSpacing(0);
    layout->addWidget(content);
    layout->addStretch();
    content->setFixedWidth(PageWidth);
    content->installEventFilter(this);

    if (inPanel) {
        // SubPluginPage.qml: a 20px back button, 8px off the left and 10px
        // off the top, and the title in the middle
        m_backButton = new V23CircleButton(V23CircleButton::Flat, 20, QSize(12, 12), this);
        m_backButton->setIcon(QStringLiteral("arrow-left"));
        m_backButton->move(8, 10);
        connect(m_backButton, &V23CircleButton::clicked, this, &V23DetailPage::back);
    }

    updateHeight();
}

void V23DetailPage::setMinimumPageHeight(int height)
{
    m_minimumHeight = height;
    updateHeight();
}

bool V23DetailPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_content && event->type() == QEvent::LayoutRequest)
        QMetaObject::invokeMethod(this, &V23DetailPage::updateHeight, Qt::QueuedConnection);

    return QWidget::eventFilter(watched, event);
}

void V23DetailPage::updateHeight()
{
    const int contentHeight = m_content->sizeHint().height();
    m_content->setFixedHeight(contentHeight);
    // never shorter than the panel it replaces, at most 600 for the content
    const int height = m_headerHeight + qMin(contentHeight, 600) + 2 * PanelSpacing;
    setFixedHeight(qMax(height, m_minimumHeight));
}

void V23DetailPage::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    if (!m_backButton)
        return;

    QPainter painter(this);
    const QFont font = pixelFont(14);
    painter.setPen(popupTextColor());
    painter.setFont(font);
    const QRect titleRect(40, 10, width() - 80, 20);
    painter.drawText(titleRect, Qt::AlignCenter, QFontMetrics(font).elidedText(m_title, Qt::ElideRight, titleRect.width()));
}
