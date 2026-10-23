// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23QUICKWIDGETS_H
#define V23QUICKWIDGETS_H

#include "v23quickstyle.h"

#include <QAbstractButton>
#include <QScrollArea>
#include <QSlider>
#include <QWidget>

class QTimer;
class QVBoxLayout;

namespace V23Quick {
constexpr int PanelWidth = 330;     // 4 tiles of 70 and their spacing
constexpr int PanelSpacing = 10;
constexpr int TileWidth = 70;
constexpr int LargeTileWidth = 150;
constexpr int FullTileWidth = 310;
constexpr int TileHeight = 60;
constexpr int PageWidth = PanelWidth - 2 * PanelSpacing;
}

// An icon of the plugin resources, as a widget keeps it
struct V23Icon
{
    QString name;
    QSize size = QSize(16, 16);
    V23Quick::IconMode mode = V23Quick::Mask;

    bool isNull() const { return name.isEmpty(); }
    QPixmap pixmap(const QColor &color, qreal devicePixelRatio) const;
    bool operator==(const V23Icon &other) const
    {
        return name == other.name && size == other.size && mode == other.mode;
    }
};

// Icons on the dock in a row, or a column on a vertical dock, in the dock
// text color; the wheel is passed on
class V23DockIcon : public QWidget
{
    Q_OBJECT

public:
    explicit V23DockIcon(QWidget *parent = nullptr);

    void setIcons(const QList<V23Icon> &icons);
    void setIcon(const V23Icon &icon) { setIcons({icon}); }
    QSize sizeHint() const override;

signals:
    void wheelScrolled(int delta) const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    QList<V23Icon> m_icons;
};

// The rounded card behind each item of the quick panel; brighter while
// hovered, pressed or when its feature is active. Clicking it emits clicked().
class V23QuickCard : public QWidget
{
    Q_OBJECT

public:
    explicit V23QuickCard(QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

signals:
    void clicked() const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    bool m_active;
    bool m_hover;
    bool m_pressed;
};

// A round button with an icon
class V23CircleButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum Style {
        Switch,     // highlighted while its feature is on
        Display,    // only the icon
        Plain,      // a faint circle, e.g. the device button of a slider row
        Flat,       // a rounded square showing on hover, e.g. the back button
    };

    V23CircleButton(Style style, int diameter, const QSize &iconSize, QWidget *parent = nullptr);

    void setStyle(Style style);
    void setIcon(const QString &name, V23Quick::IconMode mode = V23Quick::Mask);
    void setActive(bool active);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    Style m_style;
    int m_diameter;
    V23Icon m_icon;
    bool m_active;
    bool m_hover;
};

// A tile of the quick panel. A large one takes two columns: its icon in a
// circle, the name and the state, and an arrow to its page; a standard one
// is an icon above its name.
class V23QuickTile : public V23QuickCard
{
    Q_OBJECT

public:
    enum Style {
        Large,
        Standard,
    };

    explicit V23QuickTile(Style style, QWidget *parent = nullptr);

    Style style() const { return m_style; }
    void setIcon(const QString &name, V23Quick::IconMode mode = V23Quick::Mask);
    // the icon is on: a highlighted circle, or a highlighted standard tile
    void setIconActive(bool active);
    // the icon is no switch, it shows without a circle
    void setIconDisplayOnly(bool displayOnly);
    void setTitle(const QString &title);
    void setDescription(const QString &description);

signals:
    void iconClicked() const;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    Style m_style;
    V23CircleButton *m_button;
    V23Icon m_icon;
    bool m_iconActive;
    QString m_title;
    QString m_description;
};

// The slider of the quick panel: a thin line and a round handle
class V23Slider : public QSlider
{
    Q_OBJECT

public:
    explicit V23Slider(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    int valueAt(int x) const;
    QRect grooveRect() const;
};

// An icon, a slider and a button. In the quick panel it is a full width card
// with a round button that opens the page; in a page it is a slim bar.
class V23SliderRow : public V23QuickCard
{
    Q_OBJECT

public:
    enum Style {
        Panel,
        Page,
    };

    explicit V23SliderRow(Style style, QWidget *parent = nullptr);

    void setLeftIcon(const QString &name);
    void setLeftClickable(bool clickable);
    void setRightIcon(const QString &name);
    void setRange(int minimum, int maximum);
    void setValue(int value);
    int value() const;

signals:
    void valueChanged(int value) const;
    void leftClicked() const;
    void rightClicked() const;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    Style m_style;
    V23CircleButton *m_leftButton;
    V23Slider *m_slider;
    V23CircleButton *m_rightButton;
};

// A two state switch
class V23Switch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit V23Switch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
};

// The title of a part of a page, with an optional switch or text on the right
class V23SectionHeader : public QWidget
{
    Q_OBJECT

public:
    V23SectionHeader(const QString &title, bool withSwitch, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setRightText(const QString &text);
    void setChecked(bool checked);
    QSize sizeHint() const override;

signals:
    void toggled(bool checked) const;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_title;
    QString m_rightText;
    V23Switch *m_switch;
};

// An entry of a page: an icon and a text on a faint card that highlights on
// hover, and on the right a state text, a check mark, a spinner, or an arrow
class V23ListItem : public QAbstractButton
{
    Q_OBJECT

public:
    explicit V23ListItem(QWidget *parent = nullptr);

    void setIcon(const V23Icon &icon);
    void setStatusText(const QString &text);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void setBusy(bool busy);
    // an arrow on the right, for the entries that open the control center
    void setArrow(bool arrow);
    // shown on hover over a selected entry, clicking it emits actionClicked()
    void setHoverAction(const QString &text);
    QSize sizeHint() const override;

signals:
    void actionClicked() const;

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    QRect actionRect() const;

private:
    V23Icon m_icon;
    QString m_statusText;
    QString m_hoverAction;
    bool m_selected;
    bool m_busy;
    bool m_arrow;
    bool m_hover;
    int m_busyAngle;
    QTimer *m_busyTimer;
};

// A list of entries that scrolls past a maximum height
class V23ListView : public QScrollArea
{
    Q_OBJECT

public:
    explicit V23ListView(int maximumHeight, QWidget *parent = nullptr);

    QVBoxLayout *listLayout() const { return m_layout; }
    void clear();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateHeight();

private:
    int m_maximumHeight;
    QWidget *m_content;
    QVBoxLayout *m_layout;
};

// A page in the quick panel, below a back button and its title; or alone in
// the popup of a dock icon
class V23DetailPage : public QWidget
{
    Q_OBJECT

public:
    V23DetailPage(const QString &title, QWidget *content, bool inPanel, QWidget *parent = nullptr);

    void setMinimumPageHeight(int height);

signals:
    void back() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *e) override;

private:
    void updateHeight();

private:
    QString m_title;
    QWidget *m_content;
    V23CircleButton *m_backButton;
    int m_headerHeight;
    int m_minimumHeight;
};

#endif // V23QUICKWIDGETS_H
