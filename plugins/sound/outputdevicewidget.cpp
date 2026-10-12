/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "outputdevicewidget.h"
#include "dbus/dbusaudio.h"
#include "../widgets/tipswidget.h"
#include "componments/horizontalseparator.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QDBusInterface>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QRegularExpression>

#define WIDTH             200
#define SINK_INTERFACE    "com.deepin.daemon.Audio.Sink"
#define PROPERTIES_IFACE  "org.freedesktop.DBus.Properties"
#define OUTPUT_DIRECTION  1   // 1 = 播放/输出方向

// 同步读取某个 D-Bus 对象的标量属性。
// 之所以用同步 Get 而非 DBusSink 代理，是因为代理属性由 GetAll 异步回填，
// 构造后立即读取可能为空。
static QVariant readProperty(const QString &service, const QString &path,
                             const QString &interface, const QString &name)
{
    QDBusInterface props(service, path, QStringLiteral(PROPERTIES_IFACE),
                         QDBusConnection::sessionBus());
    QDBusReply<QVariant> reply = props.call(QStringLiteral("Get"), interface, name);
    if (!reply.isValid())
        return QVariant();

    return reply.value();
}

// 读取 Sink 的 ActivePort 结构体 (ssy)，返回其端口名（第一个字段）
static QString readActivePort(const QString &service, const QString &path)
{
    QDBusInterface props(service, path, QStringLiteral(PROPERTIES_IFACE),
                         QDBusConnection::sessionBus());
    QDBusMessage msg = props.call(QStringLiteral("Get"),
                                  QStringLiteral(SINK_INTERFACE),
                                  QStringLiteral("ActivePort"));
    if (msg.type() != QDBusMessage::ReplyMessage || msg.arguments().isEmpty())
        return QString();

    const QDBusVariant variant = msg.arguments().at(0).value<QDBusVariant>();
    const QDBusArgument arg = variant.variant().value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::StructureType)
        return QString();

    QString portName;
    QString portDescription;
    uchar direction = 0;
    arg.beginStructure();
    arg >> portName >> portDescription >> direction;
    arg.endStructure();

    return portName;
}

OutputDeviceWidget::OutputDeviceWidget(DBusAudio *audioInter, QWidget *parent)
    : QWidget(parent),
      m_audioInter(audioInter),
      m_listLayout(new QVBoxLayout),
      m_group(new QButtonGroup(this))
{
    m_group->setExclusive(true);

    TipsWidget *titleLabel = new TipsWidget;
    titleLabel->setText(tr("Output Device"));

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(new HorizontalSeparator);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);

    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(2);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(titleLayout);
    mainLayout->addSpacing(8);
    mainLayout->addLayout(m_listLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setLayout(mainLayout);
    setFixedWidth(WIDTH);

    // 切换默认输出设备后 DefaultSink 会变化，据此刷新列表高亮
    connect(m_audioInter, &DBusAudio::DefaultSinkChanged, this, [this] {
        refreshSinks();
    });

    // GetAll 异步回填属性、初始值不触发信号，故延迟一帧再首次枚举
    QMetaObject::invokeMethod(this, [this] { refreshSinks(); }, Qt::QueuedConnection);
}

void OutputDeviceWidget::refreshSinks()
{
    // 先销毁旧的互斥按钮组，避免其内部持有即将删除的按钮指针
    if (m_group) {
        delete m_group;
        m_group = nullptr;
    }
    // 清空旧列表
    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
    // 重建互斥按钮组
    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    const QString service = m_audioInter->service();
    const QString audioPath = m_audioInter->path();
    const QString defaultSinkPath = m_audioInter->defaultSink().path();

    // 该守护进程不提供 Sinks 属性，改为 Introspect 枚举 Audio 对象下的 SinkNN 子节点
    QDBusInterface introspectable(service, audioPath,
                                  QStringLiteral("org.freedesktop.DBus.Introspectable"),
                                  QDBusConnection::sessionBus());
    QDBusReply<QString> introspect = introspectable.call(QStringLiteral("Introspect"));
    if (!introspect.isValid())
        return;

    const QRegularExpression sinkNode(QStringLiteral("name=\"(Sink[0-9]+)\""));
    QRegularExpressionMatchIterator it = sinkNode.globalMatch(introspect.value());

    // 为图标和边距预留宽度，避免文字过长顶出窗口
    const int available = qMax(0, (width() > 0 ? width() : WIDTH) - 46);

    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString sinkPath = audioPath + "/" + match.captured(1);

        const QString name = readProperty(service, sinkPath, QStringLiteral(SINK_INTERFACE),
                                          QStringLiteral("Name")).toString();
        QString description = readProperty(service, sinkPath, QStringLiteral(SINK_INTERFACE),
                                           QStringLiteral("Description")).toString();
        if (description.isEmpty())
            description = name;
        const uint card = readProperty(service, sinkPath, QStringLiteral(SINK_INTERFACE),
                                       QStringLiteral("Card")).toUInt();
        const QString portName = readActivePort(service, sinkPath);

        if (portName.isEmpty())
            continue;

        QPushButton *button = new QPushButton;
        button->setCheckable(true);
        button->setChecked(sinkPath == defaultSinkPath);
        button->setFlat(true);
        button->setIcon(QIcon::fromTheme("audio-card",
                                         QIcon::fromTheme("multimedia-audio-player")));
        button->setText(button->fontMetrics().elidedText(description, Qt::ElideRight, available));
        // 靠左排列，像一个列表项
        button->setStyleSheet("text-align:left;");

        m_listLayout->addWidget(button);
        m_group->addButton(button);

        connect(button, &QPushButton::clicked, this, [this, card, portName]() {
            // 该守护进程无 SetDefaultSink，通过设置声卡输出端口来切换输出设备
            m_audioInter->SetPort(card, portName, OUTPUT_DIRECTION);
        });
    }

    emit contentChanged();
}
