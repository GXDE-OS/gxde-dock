// SPDX-FileCopyrightText: 2026 GXDE OS
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef V23BASICMODULES_H
#define V23BASICMODULES_H

#include "v23quickmodule.h"

#include <QPointer>

class V23DBusObject;
class QVBoxLayout;
class V23SliderRow;
class QTimer;

// The battery: an icon on the dock, the power settings on click
class V23PowerModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23PowerModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("power"); }
    QString displayName() const override;
    bool isAvailable() const override;

    V23Icon dockIcon() const override;
    QString tipText() const override;
    QString dockCommand() const override;
    MenuItems menuItems() const override;
    void invokeMenuItem(const QString &id) override;

private:
    int percentage() const;
    uint batteryState() const;
    bool plugged() const;

private:
    V23DBusObject *m_power;
    V23DBusObject *m_systemPower;
    QTimer *m_preChargeTimer;
    bool m_available;
};

// The notification center
class V23NotificationModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23NotificationModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("notification"); }
    QString displayName() const override;

    V23Icon dockIcon() const override;
    QString tipText() const override;
    void aboutToShowTip() override;
    QString dockCommand() const override;
    MenuItems menuItems() const override;
    void invokeMenuItem(const QString &id) override;

private slots:
    void updateCount();

private:
    V23DBusObject *m_notification;
    int m_count;
};

// The virtual keyboard, a switch tile
class V23OnboardModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23OnboardModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("onboard"); }
    QString displayName() const override;
    bool isAvailable() const override;

    V23Icon dockIcon() const override;
    QString dockCommand() const override;
    QWidget *createTile(QWidget *parent) override;
};

// The brightness of the primary screen, a slider row
class V23BrightnessModule : public V23QuickModule
{
    Q_OBJECT

public:
    explicit V23BrightnessModule(QObject *parent = nullptr);

    QString key() const override { return QStringLiteral("brightness"); }
    QString displayName() const override;
    bool isAvailable() const override;
    bool isDockable() const override { return false; }

    QWidget *createRow(QWidget *parent) override;
    int rowOrder() const override { return 2; }
    bool hasPage() const override { return true; }
    QWidget *createPage(QWidget *parent) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool usesWlcom() const;
    void refreshWlcom();
    void refresh();
    QMap<QString, double> brightnessMap() const;
    QString primary() const;
    double brightness() const;
    QString monitorIcon() const;
    void setBrightness(const QString &monitor, int value);
    void updateRow();
    void updatePages();

private:
    V23DBusObject *m_display;
    QPointer<V23SliderRow> m_row;
    struct Page {
        QPointer<QVBoxLayout> layout;
        QMap<QString, QPointer<V23SliderRow>> rows;
    };
    QList<Page> m_pages;
    QMap<QString, double> m_wlcomBrightness;
    QString m_wlcomPrimary;
    bool m_available;
};

#endif // V23BASICMODULES_H
