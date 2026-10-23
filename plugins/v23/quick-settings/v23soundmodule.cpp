// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "v23soundmodule.h"
#include "v23dbusobject.h"

#include <DDBusSender>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <cmath>

namespace {
const QString AudioService = QStringLiteral("com.deepin.daemon.Audio");

// the name of the (ssy) ActivePort of a sink
QString activePortName(const QVariant &value)
{
    if (!value.canConvert<QDBusArgument>())
        return QString();

    QString name;
    QString description;
    uchar available = 0;
    const QDBusArgument argument = value.value<QDBusArgument>();
    argument.beginStructure();
    argument >> name >> description >> available;
    argument.endStructure();
    return name;
}
}

V23SoundModule::V23SoundModule(QObject *parent)
    : V23QuickModule(parent)
    , m_audio(new V23DBusObject(QDBusConnection::sessionBus(), AudioService,
                                "/com/deepin/daemon/Audio", "com.deepin.daemon.Audio", this))
    , m_sink(nullptr)
{
    connect(m_audio, &V23DBusObject::propertiesChanged, this, [this](const QStringList &names) {
        if (names.contains("DefaultSink"))
            updateSink();
        if (names.contains("Cards"))
            updateDeviceLists();
        updateRows();
    });
}

QString V23SoundModule::displayName() const
{
    return tr("Sound");
}

void V23SoundModule::updateSink()
{
    const QString path = m_audio->property("DefaultSink").value<QDBusObjectPath>().path();
    if (m_sink && m_sink->path() == path)
        return;

    delete m_sink;
    m_sink = nullptr;
    if (path.isEmpty() || path == "/")
        return;

    m_sink = new V23DBusObject(QDBusConnection::sessionBus(), AudioService, path,
                               "com.deepin.daemon.Audio.Sink", this);
    connect(m_sink, &V23DBusObject::propertiesChanged, this, [this](const QStringList &names) {
        updateRows();
        if (names.contains("ActivePort") || names.contains("Card"))
            updateDeviceLists();
        emit changed();
    });
}

double V23SoundModule::volume() const
{
    return m_sink ? m_sink->property("Volume").toDouble() : 0;
}

double V23SoundModule::maxVolume() const
{
    const double max = m_audio->property("MaxUIVolume").toDouble();
    return max > 0 ? max : 1.0;
}

bool V23SoundModule::muted() const
{
    return !m_sink || m_sink->property("Mute").toBool();
}

QString V23SoundModule::volumeIcon() const
{
    // SoundQuickPanel::leftIcon(), against the maximum volume
    const double value = volume();
    if (muted())
        return QStringLiteral("audio-volume-muted-symbolic");
    if (int(std::round(value * 100)) == 0)
        return QStringLiteral("audio-volume-off-symbolic");
    if (value / maxVolume() > 0.6)
        return QStringLiteral("audio-volume-high-symbolic");
    if (value / maxVolume() > 0.3)
        return QStringLiteral("audio-volume-medium-symbolic");
    return QStringLiteral("audio-volume-low-symbolic");
}

// the kinds of ports of SoundCardPort, guessed from GXDE's names
static QString portIcon(const QString &cardName, const QString &portName, const QString &description)
{
    const QString text = (cardName + ' ' + portName + ' ' + description).toLower();
    if (text.contains("bluez") || text.contains("a2dp") || text.contains("bluetooth"))
        return QStringLiteral("sound_bluetooth");
    if (text.contains("hdmi") || text.contains("displayport"))
        return QStringLiteral("sound_hdmi");
    if (text.contains("headphone") || text.contains("headset"))
        return QStringLiteral("sound_headphone");
    if (text.contains("speaker"))
        return QStringLiteral("sound_speaker");
    return QStringLiteral("sound_other");
}

QString V23SoundModule::deviceIcon() const
{
    const QString activePort = m_sink ? activePortName(m_sink->property("ActivePort")) : QString();
    const uint activeCard = m_sink ? m_sink->property("Card").toUInt() : 0;
    for (const QJsonValue &cardValue : QJsonDocument::fromJson(m_audio->property("Cards").toString().toUtf8()).array()) {
        const QJsonObject card = cardValue.toObject();
        if (uint(card.value("Id").toInt()) != activeCard)
            continue;
        for (const QJsonValue &portValue : card.value("Ports").toArray()) {
            const QJsonObject port = portValue.toObject();
            if (port.value("Name").toString() == activePort)
                return portIcon(card.value("Name").toString(), activePort, port.value("Description").toString());
        }
    }

    return QStringLiteral("sound_other");
}

V23Icon V23SoundModule::dockIcon() const
{
    return V23Icon { volumeIcon(), QSize(16, 16) };
}

QString V23SoundModule::tipText() const
{
    if (muted())
        return tr("Mute");

    return tr("Volume %1").arg(QString::number(int(std::round(volume() * 100))) + '%');
}

V23QuickModule::MenuItems V23SoundModule::menuItems() const
{
    return {
        { QStringLiteral("mute"), muted() ? tr("Unmute") : tr("Mute") },
        { QStringLiteral("settings"), tr("Sound settings") },
    };
}

void V23SoundModule::invokeMenuItem(const QString &id)
{
    if (id == "mute") {
        toggleMute();
    } else if (id == "settings") {
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("ShowModule"))
            .arg(QString("sound"))
            .call();
    }
}

void V23SoundModule::setVolume(double value)
{
    if (!m_sink)
        return;

    m_sink->call("SetVolume", {qBound(0.0, value, maxVolume()), true});
    if (m_sink->property("Mute").toBool())
        m_sink->call("SetMute", {false});
}

void V23SoundModule::toggleMute()
{
    if (m_sink)
        m_sink->call("SetMute", {!m_sink->property("Mute").toBool()});
}

void V23SoundModule::dockWheel(int delta)
{
    if (delta != 0)
        setVolume(volume() + (delta > 0 ? 0.02 : -0.02));
}

void V23SoundModule::updateRows()
{
    for (const QPointer<V23SliderRow> &row : m_rows) {
        if (!row)
            continue;

        row->setRange(0, int(std::round(maxVolume() * 100)));
        row->setValue(int(std::round(volume() * 100)));
        row->setLeftIcon(volumeIcon());
        if (row == m_panelRow)
            row->setRightIcon(deviceIcon());
        row->setEnabled(m_sink != nullptr);
    }

    for (const QPointer<V23SectionHeader> &header : m_volumeHeaders) {
        if (header)
            header->setRightText(QString("%1%").arg(int(std::round(volume() * 100))));
    }
}

V23SliderRow *V23SoundModule::createSliderRow(V23SliderRow::Style style, QWidget *parent)
{
    V23SliderRow *row = new V23SliderRow(style, parent);
    row->setLeftClickable(true);
    m_rows << row;

    connect(row, &V23SliderRow::valueChanged, this, [this](int value) {
        setVolume(value / 100.0);
    });
    connect(row, &V23SliderRow::leftClicked, this, &V23SoundModule::toggleMute);

    updateRows();
    return row;
}

QWidget *V23SoundModule::createRow(QWidget *parent)
{
    m_panelRow = createSliderRow(V23SliderRow::Panel, parent);
    connect(m_panelRow, &V23SliderRow::rightClicked, this, &V23SoundModule::requestPage);
    connect(m_panelRow, &V23SliderRow::clicked, this, &V23SoundModule::requestPage);
    updateRows();
    return m_panelRow;
}

// SoundApplet: the volume, the output devices and the sound settings
QWidget *V23SoundModule::createPage(QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    V23SectionHeader *volumeHeader = new V23SectionHeader(tr("Volume"), false, page);
    m_volumeHeaders << volumeHeader;
    layout->addWidget(volumeHeader);
    layout->addSpacing(5);
    layout->addWidget(createSliderRow(V23SliderRow::Page, page));
    layout->addSpacing(V23Quick::PanelSpacing);
    layout->addWidget(new V23SectionHeader(tr("Output"), false, page));
    layout->addSpacing(5);

    V23ListView *devices = new V23ListView(280, page);
    m_deviceLists << devices;
    layout->addWidget(devices);
    layout->addSpacing(V23Quick::PanelSpacing);

    V23ListItem *settings = new V23ListItem(page);
    settings->setText(tr("Sound settings"));
    settings->setArrow(true);
    layout->addWidget(settings);
    connect(settings, &V23ListItem::clicked, this, [this] {
        emit requestHidePopup();
        invokeMenuItem("settings");
    });

    updateRows();
    updateDeviceLists();
    return page;
}

void V23SoundModule::updateDeviceLists()
{
    const QJsonArray cards = QJsonDocument::fromJson(m_audio->property("Cards").toString().toUtf8()).array();
    const uint activeCard = m_sink ? m_sink->property("Card").toUInt() : 0;
    const QString activePort = m_sink ? activePortName(m_sink->property("ActivePort")) : QString();

    for (const QPointer<V23ListView> &list : m_deviceLists) {
        if (!list)
            continue;

        list->clear();
        QVBoxLayout *layout = list->listLayout();

        for (const QJsonValue &cardValue : cards) {
            const QJsonObject card = cardValue.toObject();
            const uint cardId = card.value("Id").toInt();
            for (const QJsonValue &portValue : card.value("Ports").toArray()) {
                const QJsonObject port = portValue.toObject();
                // outputs only
                if (port.value("Direction").toInt() != 1)
                    continue;

                const QString portName = port.value("Name").toString();
                const QString description = port.value("Description").toString();
                V23ListItem *item = new V23ListItem(list->widget());
                item->setIcon(V23Icon { portIcon(card.value("Name").toString(), portName, description) });
                item->setText(QString("%1(%2)").arg(description, card.value("Name").toString()));
                item->setSelected(cardId == activeCard && portName == activePort);
                layout->addWidget(item);

                connect(item, &V23ListItem::clicked, this, [this, cardId, portName] {
                    m_audio->call("SetPort", {cardId, portName, 1});
                });
            }
        }
    }
}
