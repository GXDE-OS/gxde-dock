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

#include "sinkinputwidget.h"
#include "../widgets/tipswidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QApplication>
#include <DHiDPIHelper>

DWIDGET_USE_NAMESPACE

#define ICON_SIZE   24

const QPixmap getIconFromTheme(const QString &name, const QSize &size, const qreal ratio)
{
    // Qt6: QIcon::pixmap() already returns a DPR-aware pixmap
    return QIcon::fromTheme(name, QIcon::fromTheme("application-x-desktop")).pixmap(size);
}

SinkInputWidget::SinkInputWidget(const QString &inputPath, QWidget *parent)
    : QWidget(parent),

      m_inputInter(new DBusSinkInput(inputPath, this)),

      m_volumeIcon(new DImageButton),
      m_volumeSlider(new VolumeSlider),
      m_volumeLabel(new TipsWidget),
      m_titleLabel(new TipsWidget)
{
    const QString iconName = m_inputInter->icon();
    m_volumeIcon->setAccessibleName("app-" + iconName + "-icon");
    m_volumeIcon->setPixmap(getIconFromTheme(iconName, QSize(ICON_SIZE, ICON_SIZE), devicePixelRatioF()));
    m_volumeIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_volumeSlider->setAccessibleName("app-" + iconName + "-slider");
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(1000);

    m_volumeLabel->setText(QString("%1%").arg(int(m_inputInter->volume() * 100)));
    m_titleLabel->setText(m_inputInter->name());

    // 应用图标 + 名称 + 音量数值（单独一行）
    QHBoxLayout *appLayout = new QHBoxLayout;
    appLayout->setAlignment(Qt::AlignLeft);
    // 与下方拖动条的缩进对齐（图标 24 + 间距 10）
    appLayout->addSpacing(2);
    appLayout->addWidget(m_volumeIcon);
    appLayout->addSpacing(10);
    appLayout->addWidget(m_titleLabel);
    appLayout->addStretch();
    appLayout->addWidget(m_volumeLabel, 0, Qt::AlignRight);
    appLayout->setSpacing(0);
    appLayout->setContentsMargins(0, 0, 0, 0);

    // 拖动条（单独一行）
    QHBoxLayout *volumeCtrlLayout = new QHBoxLayout;
    volumeCtrlLayout->addSpacing(2);
    volumeCtrlLayout->addWidget(m_volumeSlider);
    volumeCtrlLayout->setSpacing(0);
    volumeCtrlLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addLayout(appLayout);
    centralLayout->addSpacing(6);
    centralLayout->addLayout(volumeCtrlLayout);
    centralLayout->setSpacing(2);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    connect(m_volumeSlider, &VolumeSlider::valueChanged, this, &SinkInputWidget::setVolume);
    connect(m_volumeSlider, &VolumeSlider::requestPlaySoundEffect, this, &SinkInputWidget::onPlaySoundEffect);
    connect(m_volumeIcon, &DImageButton::clicked, this, &SinkInputWidget::setMute);
    connect(m_inputInter, &DBusSinkInput::MuteChanged, this, &SinkInputWidget::setMuteIcon);
    connect(m_inputInter, &DBusSinkInput::VolumeChanged, this, [=] {
        m_volumeSlider->setValue(m_inputInter->volume() * 1000);
        m_volumeLabel->setText(QString::number(int(m_inputInter->volume() * 100)) + '%');
        updateTitle();
    });

    setLayout(centralLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(60);

    setMuteIcon();

    emit m_inputInter->VolumeChanged();
}

void SinkInputWidget::updateTitle()
{
    // 应用名按标题控件自己的字体、这一行实际剩余的宽度截断，
    // 否则会和右侧的百分比重叠
    const int available = width() - 2 - ICON_SIZE - 10 - m_volumeLabel->width() - 8;
    m_titleLabel->setText(m_titleLabel->fontMetrics().elidedText(
        m_inputInter->name(), Qt::ElideRight, qMax(0, available)));
}

void SinkInputWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTitle();
}

void SinkInputWidget::setVolume(const int value)
{
    m_inputInter->SetVolumeQueued(double(value) / 1000.0, false);
    m_volumeLabel->setText(QString("%1%").arg(value / 10));
}

void SinkInputWidget::setMute()
{
    m_inputInter->SetMuteQueued(!m_inputInter->mute());
}

void SinkInputWidget::setMuteIcon()
{
    if (m_inputInter->mute()) {
        const auto ratio = devicePixelRatioF();
        QPixmap muteIcon = DHiDPIHelper::loadNxPixmap("://audio-volume-muted-symbolic.svg");
        QPixmap appIconSource(getIconFromTheme(m_inputInter->icon(), QSize(24, 24), devicePixelRatioF()));

        QPixmap temp(appIconSource.size());
        temp.fill(Qt::transparent);
        temp.setDevicePixelRatio(ratio);
        QPainter p1(&temp);
        p1.drawPixmap(0, 0, appIconSource);
        p1.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p1.fillRect(temp.rect(), QColor(0, 0, 0, 40));
        p1.end();
        appIconSource = temp;

        QPainter p(&appIconSource);
        p.drawPixmap(0, 0, muteIcon);

        appIconSource.setDevicePixelRatio(ratio);
        m_volumeIcon->setPixmap(appIconSource);
    } else {
        m_volumeIcon->setPixmap(getIconFromTheme(m_inputInter->icon(), QSize(24, 24), devicePixelRatioF()));
    }
}

void SinkInputWidget::onPlaySoundEffect()
{
    // set the mute property to false to play sound effects.
    m_inputInter->SetMuteQueued(false);
}
