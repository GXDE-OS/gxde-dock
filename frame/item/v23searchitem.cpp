/*
 * Copyright (C) 2026 CharOfString <root@charofstring.cc>
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

#include "v23searchitem.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>

namespace {
const QString GrandSearchService = "com.deepin.dde.GrandSearch";
const QString GrandSearchPath = "/com/deepin/dde/GrandSearch";
const QString GrandSearchInterface = "com.deepin.dde.GrandSearch";
const QString SearchConfigId = "search-config";
}  // namespace

V23SearchItem::V23SearchItem(QWidget *parent)
        : DockItem(parent)
        , m_tips(new TipsWidget(this)) {
    m_tips->setText(tr("Grand Search"));
    m_tips->setVisible(false);
    m_tips->setObjectName("V23SearchItem");

    setAccessibleName("GrandSearch");
}

void V23SearchItem::paintEvent(QPaintEvent *e) {
    DockItem::paintEvent(e);

    const bool horizontal = DockPosition == Dock::Top || DockPosition == Dock::Bottom;
    const qreal iconSize = (horizontal ? height() : width()) * 9.0 / 14.0;
    QRectF icon(0, 0, iconSize, iconSize);
    icon.moveCenter(QRectF(rect()).center());

    static const QPixmap pixmap(":/icons/resources/v23-grand-search.png");
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(icon, pixmap, pixmap.rect());
}

void V23SearchItem::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::RightButton) {
        m_popupTipsDelayTimer->stop();
        hideNonModel();
        return showContextMenu();
    }

    DockItem::mousePressEvent(e);
}

void V23SearchItem::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !rect().contains(e->pos())) {
        return DockItem::mouseReleaseEvent(e);
    }

    hidePopup();
    QDBusMessage message = QDBusMessage::createMethodCall(GrandSearchService, GrandSearchPath,
        GrandSearchInterface, "SetVisible");
    message << true;
    QDBusConnection::sessionBus().asyncCall(message);
}

QSize V23SearchItem::sizeHint() const {
    return QSize(40, 40);
}

QWidget *V23SearchItem::popupTips() {
    return m_tips;
}

const QString V23SearchItem::contextMenu() const {
    QJsonObject config;
    config.insert("itemId", SearchConfigId);
    config.insert("itemText", tr("Search settings"));
    config.insert("isActive", true);

    QJsonObject menu;
    menu.insert("items", QJsonArray { config });
    menu.insert("checkableMenu", false);
    menu.insert("singleCheck", false);

    return QJsonDocument(menu).toJson();
}

void V23SearchItem::invokedMenuItem(const QString &itemId, const bool checked) {
    Q_UNUSED(checked);

    if (itemId == SearchConfigId) {
        QProcess::startDetached("dde-grand-search", { "--setting" });
    }
}
