/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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

#include <QDebug>

#include "dockdaemoninterface.h"
#include "dockdbusnames.h"

DockDaemonInterface::DockDaemonInterface(const QString& service,
        const QString& path, const QDBusConnection& connection,
        QObject* parent) : DBusExtendedAbstractInterface(service, path,
            dockDBusManagerInterface(), connection, parent), m_displayMode(0)
        , m_hideMode(0), m_hideState(0), m_iconSize(0), m_opacity(0)
        , m_position(0), m_showTimeout(0), m_windowSplit(false) {
    connect(this, &DockDaemonInterface::propertyChanged,
            this, &DockDaemonInterface::onPropertyChanged);
}

DockDaemonInterface::~DockDaemonInterface() {
    qInfo() << "(DockDaemon) Destory: Dock daemon interface class destoryed!";
}

int DockDaemonInterface::displayMode() {
    return qvariant_cast<int>(internalPropGet("DisplayMode", &m_displayMode));
}

void DockDaemonInterface::setDisplayMode(int value) {
    internalPropSet("DisplayMode", QVariant::fromValue(value), &m_displayMode);
}

QList<QDBusObjectPath> DockDaemonInterface::entries() {
    return qvariant_cast<QList<QDBusObjectPath>>(internalPropGet("Entries",
        &m_entries));
}

int DockDaemonInterface::hideMode() {
    return qvariant_cast<int>(internalPropGet("HideMode", &m_hideMode));
}

void DockDaemonInterface::setHideMode(int value) {
    internalPropSet("HideMode", QVariant::fromValue(value), &m_hideMode);
}

int DockDaemonInterface::hideState() {
    return qvariant_cast<int>(internalPropGet("HideState", &m_hideState));
}

uint DockDaemonInterface::iconSize() {
    return qvariant_cast<uint>(internalPropGet("IconSize", &m_iconSize));
}

void DockDaemonInterface::setIconSize(uint value) {
    internalPropSet("IconSize", QVariant::fromValue(value), &m_iconSize);
}

double DockDaemonInterface::opacity() {
    return qvariant_cast<double>(internalPropGet("Opacity", &m_opacity));
}

int DockDaemonInterface::position() {
    return qvariant_cast<int>(internalPropGet("Position", &m_position));
}

void DockDaemonInterface::setPosition(int value) {
    internalPropSet("Position", QVariant::fromValue(value), &m_position);
}

uint DockDaemonInterface::showTimeout() {
    return qvariant_cast<uint>(internalPropGet("ShowTimeout", &m_showTimeout));
}

void DockDaemonInterface::setShowTimeout(uint value) {
    internalPropSet("ShowTimeout", QVariant::fromValue(value), &m_showTimeout);
}

bool DockDaemonInterface::windowSplit() {
    return qvariant_cast<bool>(internalPropGet("WindowSplit", &m_windowSplit));
}

void DockDaemonInterface::setWindowSplit(bool value) {
    internalPropSet("WindowSplit", QVariant::fromValue(value), &m_windowSplit);
}

void DockDaemonInterface::onPropertyChanged(const QString& propName,
        const QVariant& value) {
    if (propName == QLatin1String("DisplayMode")) {
        const int newValue = qvariant_cast<int>(value);
        if (m_displayMode != newValue) {
            m_displayMode = newValue;
            Q_EMIT DisplayModeChanged(newValue);
        }
    } else if (propName == QLatin1String("Entries")) {
        const QList<QDBusObjectPath> newValue = qvariant_cast<QList<
            QDBusObjectPath>>(value);

        if (m_entries != newValue) {
            m_entries = newValue;
            Q_EMIT EntriesChanged(newValue);
        }
    } else if (propName == QLatin1String("HideMode")) {
        const int newValue = qvariant_cast<int>(value);
        if (m_hideMode != newValue) {
            m_hideMode = newValue;
            Q_EMIT HideModeChanged(newValue);
        }
    } else if (propName == QLatin1String("HideState")) {
        const int newValue = qvariant_cast<int>(value);
        if (m_hideState != newValue) {
            m_hideState = newValue;
            Q_EMIT HideStateChanged(newValue);
        }
    } else if (propName == QLatin1String("IconSize")) {
        const uint newValue = qvariant_cast<uint>(value);
        if (m_iconSize != newValue) {
            m_iconSize = newValue;
            Q_EMIT IconSizeChanged(newValue);
        }
    } else if (propName == QLatin1String("Opacity")) {
        const double newValue = qvariant_cast<double>(value);
        if (!qFuzzyCompare(m_opacity, newValue)) {
            m_opacity = newValue;
            Q_EMIT OpacityChanged(newValue);
        }
    } else if (propName == QLatin1String("Position")) {
        const int newValue = qvariant_cast<int>(value);
        if (m_position != newValue) {
            m_position = newValue;
            Q_EMIT PositionChanged(newValue);
        }
    } else if (propName == QLatin1String("ShowTimeout")) {
        const uint newValue = qvariant_cast<uint>(value);
        if (m_showTimeout != newValue) {
            m_showTimeout = newValue;
            Q_EMIT ShowTimeoutChanged(newValue);
        }
    } else if (propName == QLatin1String("WindowSplit")) {
        const bool newValue = qvariant_cast<bool>(value);
        if (m_windowSplit != newValue) {
            m_windowSplit = newValue;
            Q_EMIT WindowSplitChanged(newValue);
        }
    }
}
