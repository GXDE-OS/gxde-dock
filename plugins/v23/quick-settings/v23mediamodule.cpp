// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// The row follows MediaWidget of dde-dock 6.0.37: the cover, the title and
// the artist, and the play/pause and next buttons drawn by hand.

#include "v23mediamodule.h"
#include "v23dbusobject.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

using namespace V23Quick;

namespace {
const QString MprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");

QVariantMap toVariantMap(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>())
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    return value.toMap();
}

QStringList toStringList(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>())
        return qdbus_cast<QStringList>(value.value<QDBusArgument>());
    return value.toStringList();
}
}

// The media row of dde-tray-loader: the cover, the title over the artist,
// and the play and next buttons
class V23MediaRow : public V23QuickCard
{
public:
    explicit V23MediaRow(QWidget *parent)
        : V23QuickCard(parent)
        , playButton(new V23CircleButton(V23CircleButton::Display, 24, QSize(24, 24), this))
        , nextButton(new V23CircleButton(V23CircleButton::Display, 24, QSize(24, 24), this))
    {
        setFixedSize(FullTileWidth, TileHeight);
        nextButton->setIcon(QStringLiteral("play-next"));
        nextButton->move(width() - 18 - nextButton->width(), (height() - nextButton->height()) / 2);
        playButton->move(nextButton->x() - 26 - playButton->width(), (height() - playButton->height()) / 2);
    }

    void setInfo(const QString &title, const QString &artist, const QPixmap &cover)
    {
        m_title = title;
        m_artist = artist;
        m_cover = cover;
        update();
    }

    V23CircleButton *playButton;
    V23CircleButton *nextButton;

protected:
    void paintEvent(QPaintEvent *e) override
    {
        V23QuickCard::paintEvent(e);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QRect coverRect(14, (height() - 32) / 2, 32, 32);
        if (!m_cover.isNull()) {
            QPainterPath clip;
            clip.addRoundedRect(coverRect, 4, 4);
            painter.save();
            painter.setClipPath(clip);
            painter.drawPixmap(coverRect, m_cover);
            painter.restore();
        }

        const int left = coverRect.right() + 1 + 10;
        const int right = playButton->x() - 10;
        const QFont titleFont = pixelFont(11);
        const QFont artistFont = pixelFont(10);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics artistMetrics(artistFont);
        const int top = (height() - titleMetrics.height() - 2 - artistMetrics.height()) / 2;

        painter.setPen(popupTextColor());
        painter.setFont(titleFont);
        painter.drawText(QRect(left, top, right - left, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(m_title, Qt::ElideRight, right - left));
        painter.setPen(withAlpha(popupTextColor(), 0.7));
        painter.setFont(artistFont);
        painter.drawText(QRect(left, top + titleMetrics.height() + 2, right - left, artistMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         artistMetrics.elidedText(m_artist, Qt::ElideRight, right - left));
    }

private:
    QString m_title;
    QString m_artist;
    QPixmap m_cover;
};

V23MediaModule::V23MediaModule(QObject *parent)
    : V23QuickModule(parent)
    , m_player(nullptr)
    , m_available(false)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged",
                this, SLOT(onNameOwnerChanged(QString, QString, QString)));

    for (const QString &name : bus.interface()->registeredServiceNames().value()) {
        if (name.startsWith(MprisPrefix))
            m_players << name;
    }

    choosePlayer();
}

QString V23MediaModule::displayName() const
{
    return tr("Media");
}

bool V23MediaModule::isAvailable() const
{
    return m_available;
}

void V23MediaModule::onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner);

    if (!name.startsWith(MprisPrefix))
        return;

    m_players.removeAll(name);
    if (!newOwner.isEmpty())
        m_players << name;

    choosePlayer();
}

// the latest player that came up
void V23MediaModule::choosePlayer()
{
    const QString service = m_players.isEmpty() ? QString() : m_players.last();
    if (m_player && m_player->objectName() == service)
        return;

    delete m_player;
    m_player = nullptr;

    if (!service.isEmpty()) {
        m_player = new V23DBusObject(QDBusConnection::sessionBus(), service, "/org/mpris/MediaPlayer2",
                                     "org.mpris.MediaPlayer2.Player", this);
        m_player->setObjectName(service);
        connect(m_player, &V23DBusObject::propertiesChanged, this, [this] {
            updateAvailable();
            updateRow();
        });
    }

    updateAvailable();
    updateRow();
}

void V23MediaModule::updateAvailable()
{
    const bool available = m_player && m_player->isValid()
        && !toVariantMap(m_player->property("Metadata")).value("xesam:title").toString().isEmpty();
    if (available == m_available)
        return;

    m_available = available;
    emit availableChanged();
}

void V23MediaModule::updateRow()
{
    if (!m_row || !m_player)
        return;

    const QVariantMap metadata = toVariantMap(m_player->property("Metadata"));
    QString artist = toStringList(metadata.value("xesam:artist")).join(", ");
    if (artist.isEmpty())
        artist = tr("Unknown");

    QPixmap cover;
    const QUrl artUrl(metadata.value("mpris:artUrl").toString());
    if (artUrl.isLocalFile())
        cover = QPixmap(artUrl.toLocalFile());
    if (cover.isNull())
        cover = QIcon(QStringLiteral(":/v23-quick-settings/icons/deepin-music.svg")).pixmap(QSize(32, 32), m_row->devicePixelRatioF());

    m_row->setInfo(metadata.value("xesam:title").toString(), artist, cover);
    m_row->playButton->setIcon(m_player->property("PlaybackStatus").toString() == "Playing"
                               ? QStringLiteral("play-pause") : QStringLiteral("play-start"));
    m_row->nextButton->setEnabled(m_player->property("CanGoNext").toBool());
}

QWidget *V23MediaModule::createRow(QWidget *parent)
{
    m_row = new V23MediaRow(parent);
    connect(m_row->playButton, &QAbstractButton::clicked, this, [this] {
        if (m_player)
            m_player->call("PlayPause");
    });
    connect(m_row->nextButton, &QAbstractButton::clicked, this, [this] {
        if (m_player)
            m_player->call("Next");
    });

    updateRow();
    return m_row;
}
