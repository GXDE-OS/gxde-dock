/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#ifndef MONITORITEM_H
#define MONITORITEM_H

#include "constants.h"

#include <QPixmap>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class MonitorItem : public QWidget
{
    Q_OBJECT

public:
    // themeIconNames 为候选的主题图标基础名（按优先级排列），Efficient/V23
    // 模式下会自动尝试对应的 -symbolic 名称；fallbackIconPath 为主题图标
    // 缺失时使用的内置 svg 资源路径
    explicit MonitorItem(const QStringList &themeIconNames,
                         const QString &fallbackIconPath,
                         QWidget *parent = nullptr);

    void setDockDisplayMode(const Dock::DisplayMode mode);
    void refreshIcon();

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    QSize sizeHint() const override;

private slots:
    void blink();

private:
    void updateIcon();

    Dock::DisplayMode m_displayMode;
    QStringList m_themeIconNames;
    QString m_fallbackIconPath;
    // 白/红交替闪烁的两组像素图（预先生成，闪烁时仅切换引用）
    QPixmap m_icons[2];
    QPixmap m_icon;
    int m_currentIcon;
    QTimer *m_blinkTimer;
};

#endif // MONITORITEM_H
