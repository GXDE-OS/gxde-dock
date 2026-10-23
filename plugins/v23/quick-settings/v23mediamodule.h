// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23MEDIAMODULE_H
#define V23MEDIAMODULE_H

#include "v23quickmodule.h"

#include <QPointer>

class V23DBusObject;
class V23MediaRow;

// The media player playing now, through MPRIS
class V23MediaModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23MediaModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("media"); }
    QString displayName() const override;
    bool isAvailable() const override;
    bool isDockable() const override { return false; }

    QWidget *createRow(QWidget *parent) override;
    int rowOrder() const override { return 1; }

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void choosePlayer();
    void updateRow();
    void updateAvailable();

private:
    QStringList m_players;
    V23DBusObject *m_player;
    QPointer<V23MediaRow> m_row;
    bool m_available;
};

#endif // V23MEDIAMODULE_H
