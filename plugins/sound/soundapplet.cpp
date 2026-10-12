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

#include "soundapplet.h"
#include "sinkinputwidget.h"
#include "outputdevicewidget.h"
#include "componments/horizontalseparator.h"
#include "../widgets/tipswidget.h"
#include "util/utils.h"

#include <QLabel>
#include <QIcon>
#include <QApplication>
#include <QShowEvent>

#define WIDTH       200
#define ICON_SIZE   24

DWIDGET_USE_NAMESPACE

SoundApplet::SoundApplet(QWidget *parent)
    : QScrollArea(parent),

      m_centralWidget(new QWidget),
      m_applicationTitle(new QWidget),
      m_volumeBtn(new DImageButton),
      m_volumeSlider(new VolumeSlider),
      m_soundShow(new TipsWidget),

      m_audioInter(new DBusAudio(this)),
      m_defSinkInter(nullptr)
{
//    QIcon::setThemeName("deepin");

    m_volumeBtn->setAccessibleName("volume-button");
    m_volumeSlider->setAccessibleName("volume-slider");

    TipsWidget *deviceLabel = new TipsWidget;
    deviceLabel->setText(tr("Device"));

    m_soundShow->setText(QString("%1%").arg(0));

    QHBoxLayout *deviceLineLayout = new QHBoxLayout;
    deviceLineLayout->addWidget(deviceLabel);
//    deviceLineLayout->addSpacing(12);
    deviceLineLayout->addWidget(new HorizontalSeparator);
    deviceLineLayout->addWidget(m_soundShow);
    deviceLineLayout->setContentsMargins(0, 0, 0, 0);
    deviceLineLayout->setSpacing(10);

    QHBoxLayout *volumeCtrlLayout = new QHBoxLayout;
    volumeCtrlLayout->addSpacing(2);
    volumeCtrlLayout->addWidget(m_volumeBtn);
    volumeCtrlLayout->addSpacing(10);
    volumeCtrlLayout->addWidget(m_volumeSlider);
    volumeCtrlLayout->setSpacing(0);
    volumeCtrlLayout->setContentsMargins(0, 0, 0, 0);

    TipsWidget *appLabel = new TipsWidget;
    appLabel->setText(tr("Application"));

    QHBoxLayout *appLineHLayout = new QHBoxLayout;
    appLineHLayout->addWidget(appLabel);
    appLineHLayout->addWidget(new HorizontalSeparator);
    appLineHLayout->setContentsMargins(0, 0, 0, 0);
    appLineHLayout->setSpacing(10);

    QVBoxLayout *appLineVLayout = new QVBoxLayout;
    appLineVLayout->addSpacing(10);
    appLineVLayout->addLayout(appLineHLayout);
    appLineVLayout->addSpacing(8);
    appLineVLayout->setSpacing(0);
    appLineVLayout->setContentsMargins(0, 0, 0, 0);

    m_applicationTitle->setLayout(appLineVLayout);

    m_volumeBtn->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(1500);

    m_centralLayout = new QVBoxLayout;
    m_centralLayout->addLayout(deviceLineLayout);
    m_centralLayout->addSpacing(8);
    m_centralLayout->addLayout(volumeCtrlLayout);
    m_centralLayout->addWidget(m_applicationTitle);

    // 应用音量列表（动态增删），独立成一个布局，避免影响底部固定控件
    m_appLayout = new QVBoxLayout;
    m_appLayout->setSpacing(10);
    m_appLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->addLayout(m_appLayout);

    // 最下方：输出设备切换控件
    m_outputDeviceWidget = new OutputDeviceWidget(m_audioInter);
    m_centralLayout->addSpacing(10);
    m_centralLayout->addWidget(m_outputDeviceWidget);

    m_centralWidget->setLayout(m_centralLayout);
    m_centralWidget->setFixedWidth(WIDTH);
    m_centralWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    setFixedWidth(WIDTH);
    setWidget(m_centralWidget);
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("background-color:transparent;");

    connect(m_volumeBtn, &DImageButton::clicked, this, &SoundApplet::toggleMute);
    connect(m_volumeSlider, &VolumeSlider::valueChanged, this, &SoundApplet::volumeSliderValueChanged);
    connect(m_volumeSlider, &VolumeSlider::requestPlaySoundEffect, this, &SoundApplet::onPlaySoundEffect);
    connect(m_audioInter, &DBusAudio::SinkInputsChanged, this, &SoundApplet::sinkInputsChanged);
    connect(m_audioInter, &DBusAudio::DefaultSinkChanged, this, static_cast<void (SoundApplet::*)()>(&SoundApplet::defaultSinkChanged));
    connect(this, static_cast<void (SoundApplet::*)(DBusSink*) const>(&SoundApplet::defaultSinkChanged), this, &SoundApplet::onVolumeChanged);
    connect(m_outputDeviceWidget, &OutputDeviceWidget::contentChanged, this, &SoundApplet::adjustHeight);

    QMetaObject::invokeMethod(this, "defaultSinkChanged", Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "sinkInputsChanged", Qt::QueuedConnection);
}

int SoundApplet::volumeValue() const
{
    return m_volumeSlider->value();
}

VolumeSlider *SoundApplet::mainSlider()
{
    return m_volumeSlider;
}

void SoundApplet::defaultSinkChanged()
{
    delete m_defSinkInter;

    const QDBusObjectPath defSinkPath = m_audioInter->defaultSink();
    m_defSinkInter = new DBusSink(defSinkPath.path(), this);

    connect(m_defSinkInter, &DBusSink::VolumeChanged, this, &SoundApplet::onVolumeChanged);
    connect(m_defSinkInter, &DBusSink::MuteChanged, this, &SoundApplet::onVolumeChanged);

    emit defaultSinkChanged(m_defSinkInter);
}

void SoundApplet::onVolumeChanged()
{
    const float volume = m_defSinkInter->volume();
    const bool mute = m_defSinkInter->mute();

    m_volumeSlider->setValue(std::min(1500.0f, volume * 1000.0f));

    emit volumeChanged(m_volumeSlider->value());

    QString volumeString;
    if (mute)
        volumeString = "muted";
    else if (volume >= double(2)/3)
        volumeString = "high";
    else if (volume >= double(1)/3)
        volumeString = "medium";
    else
        volumeString = "low";

    const QString &iconName = QString("://audio-volume-%1-symbolic.svg").arg(volumeString);
    QPixmap pix = Utils::renderSVG(iconName, QSize(24, 24), devicePixelRatioF());

    m_volumeBtn->setPixmap(pix);

    m_soundShow->setText(QString::number(volume * 100) + '%');
}

void SoundApplet::volumeSliderValueChanged()
{
    m_defSinkInter->SetVolumeQueued(m_volumeSlider->value() / 1000.0f, false);

    m_soundShow->setText(QString("%1%").arg(m_volumeSlider->value() / 10));
}

void SoundApplet::sinkInputsChanged()
{
    m_centralWidget->setVisible(false);
    QVBoxLayout *appLayout = m_appLayout;
    while (QLayoutItem *item = appLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    m_applicationTitle->setVisible(false);
    for (auto input : m_audioInter->sinkInputs())
    {
        m_applicationTitle->setVisible(true);
        SinkInputWidget *si = new SinkInputWidget(input.path());
        appLayout->addWidget(si);
    }

    m_centralWidget->setVisible(true);
    adjustHeight();
}

void SoundApplet::adjustHeight()
{
    const int contentHeight = m_centralWidget->sizeHint().height();
    m_centralWidget->setFixedHeight(contentHeight);

    // 根据内容自动延长悬浮窗，避免滚动；
    // 上限取屏幕可用高度，防止窗口超出屏幕导致无法操作
    const int maxHeight = QApplication::primaryScreen()->availableGeometry().height();
    setFixedHeight(std::min(contentHeight, maxHeight));
}

void SoundApplet::toggleMute()
{
    m_defSinkInter->SetMuteQueued(!m_defSinkInter->mute());
}

void SoundApplet::showEvent(QShowEvent *event)
{
    QScrollArea::showEvent(event);

    // 每次打开悬浮窗时重新枚举输出设备，及时反映新插入/拔出的设备
    if (m_outputDeviceWidget)
        m_outputDeviceWidget->refreshSinks();
}

void SoundApplet::onPlaySoundEffect()
{
    // set the mute property to false to play sound effects.
    m_defSinkInter->SetMuteQueued(false);
}
