// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23QUICKMODULE_H
#define V23QUICKMODULE_H

#include "v23quickwidgets.h"

#include <QObject>
#include <QPair>

// One feature of the quick settings, e.g. the network. It may put a tile or
// a row in the quick panel, have a page of details, and show an icon on the
// dock when the user docks it.
class V23QuickModule : public QObject
{
    Q_OBJECT

public:
    using MenuItems = QList<QPair<QString, QString>>; // id, text

    explicit V23QuickModule(QObject *parent = nullptr) : QObject(parent) {}

    // also the item key of its dock icon
    virtual QString key() const = 0;
    virtual QString displayName() const = 0;
    virtual bool isAvailable() const { return true; }
    virtual bool isDockable() const { return true; }

    virtual V23Icon dockIcon() const { return V23Icon(); }
    virtual QString tipText() const { return displayName(); }
    // its tips are about to show, a chance to refresh them
    virtual void aboutToShowTip() {}
    // run when its dock icon is clicked, if it has no page
    virtual QString dockCommand() const { return QString(); }
    virtual MenuItems menuItems() const { return MenuItems(); }
    virtual void invokeMenuItem(const QString &id) { Q_UNUSED(id); }
    virtual void dockWheel(int delta) { Q_UNUSED(delta); }

    // a V23QuickTile, or a full width row ordered by rowOrder() from the top
    virtual QWidget *createTile(QWidget *parent) { Q_UNUSED(parent); return nullptr; }
    virtual QWidget *createRow(QWidget *parent) { Q_UNUSED(parent); return nullptr; }
    virtual int rowOrder() const { return 0; }

    // the details, both in the quick panel and in the popup of its dock icon
    virtual bool hasPage() const { return false; }
    virtual QWidget *createPage(QWidget *parent) { Q_UNUSED(parent); return nullptr; }

signals:
    // the dock icon or the tips changed
    void changed() const;
    void availableChanged() const;
    void requestPage() const;
    void requestHidePopup() const;
};

#endif // V23QUICKMODULE_H
