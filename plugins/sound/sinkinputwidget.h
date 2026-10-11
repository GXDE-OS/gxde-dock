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

#ifndef SINKINPUTWIDGET_H
#define SINKINPUTWIDGET_H

#include "dbus/dbussinkinput.h"
#include "componments/volumeslider.h"

#include <QFrame>
#include <QResizeEvent>

#include <dimagebutton.h>
#include <QPainter>

class TipsWidget;
class SinkInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SinkInputWidget(const QString &inputPath, QWidget *parent = 0);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void setVolume(const int value);
    void setMute();
    void setMuteIcon();
    void onPlaySoundEffect();

private:
    void updateTitle();

    DBusSinkInput *m_inputInter;

    Dtk::Widget::DImageButton *m_volumeIcon;
    VolumeSlider *m_volumeSlider;
    TipsWidget *m_volumeLabel;
    TipsWidget *m_titleLabel;
};

#endif // SINKINPUTWIDGET_H
