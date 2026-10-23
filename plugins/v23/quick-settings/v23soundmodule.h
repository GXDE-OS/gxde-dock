// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23SOUNDMODULE_H
#define V23SOUNDMODULE_H

#include "v23quickmodule.h"

#include <QPointer>

class V23DBusObject;

// The volume of the default output, and the output devices in its page
class V23SoundModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23SoundModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("sound"); }
    QString displayName() const override;

    V23Icon dockIcon() const override;
    QString tipText() const override;
    MenuItems menuItems() const override;
    void invokeMenuItem(const QString &id) override;
    void dockWheel(int delta) override;

    QWidget *createRow(QWidget *parent) override;
    int rowOrder() const override { return 3; }
    bool isDockable() const override { return false; }
    bool hasPage() const override { return true; }
    QWidget *createPage(QWidget *parent) override;

private:
    double volume() const;
    double maxVolume() const;
    bool muted() const;
    QString volumeIcon() const;
    void setVolume(double volume);
    void toggleMute();
    void updateSink();
    void updateRows();
    void updateDeviceLists();
    QString deviceIcon() const;
    V23SliderRow *createSliderRow(V23SliderRow::Style style, QWidget *parent);

private:
    V23DBusObject *m_audio;
    V23DBusObject *m_sink;
    QList<QPointer<V23SliderRow>> m_rows;
    QPointer<V23SliderRow> m_panelRow;
    QList<QPointer<V23SectionHeader>> m_volumeHeaders;
    QList<QPointer<V23ListView>> m_deviceLists;
};

#endif // V23SOUNDMODULE_H
