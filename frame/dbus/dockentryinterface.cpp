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

#include "dockentryinterface.h"
#include "dockdbusnames.h"

DockEntryInterface::DockEntryInterface(const QString& service,
        const QString& path, const QDBusConnection& connection,
        QObject* parent) : DBusExtendedAbstractInterface(service, path,
            dockDBusEntryInterface(), connection, parent)
        , m_currentWindow(0), m_isActive(false), m_isDocked(false) {
    registerWindowInfoMetaType();
    registerWindowInfoMapMetaType();

    connect(this, &DockEntryInterface::propertyChanged,
        this, &DockEntryInterface::onPropertyChanged);
}

DockEntryInterface::~DockEntryInterface() {
    qInfo() << "(DockEntry) Destory: Dock entry interface class destoryed.";
}

uint DockEntryInterface::currentWindow() {
    return qvariant_cast<uint>(internalPropGet("CurrentWindow",
        &m_currentWindow));
}

QString DockEntryInterface::desktopFile() {
    return qvariant_cast<QString>(internalPropGet("DesktopFile",
        &m_desktopFile));
}

QString DockEntryInterface::icon() {
    return qvariant_cast<QString>(internalPropGet("Icon", &m_icon));
}

QString DockEntryInterface::id() {
    return qvariant_cast<QString>(internalPropGet("Id", &m_id));
}

bool DockEntryInterface::isActive() {
    return qvariant_cast<bool>(internalPropGet("IsActive", &m_isActive));
}

bool DockEntryInterface::isDocked() {
    return qvariant_cast<bool>(internalPropGet("IsDocked", &m_isDocked));
}

QString DockEntryInterface::menu() {
    return qvariant_cast<QString>(internalPropGet("Menu", &m_menu));
}

QString DockEntryInterface::name() {
    return qvariant_cast<QString>(internalPropGet("Name", &m_name));
}

WindowInfoMap DockEntryInterface::windowInfos() {
    return qvariant_cast<WindowInfoMap>(internalPropGet(
        "WindowInfos", &m_windowInfos));
}

void DockEntryInterface::onPropertyChanged(const QString& propName,
        const QVariant& value) {
    if (propName == QLatin1String("CurrentWindow")) {
        const uint newValue = qvariant_cast<uint>(value);
        if (m_currentWindow != newValue) {
            m_currentWindow = newValue;
            Q_EMIT CurrentWindowChanged(newValue);
        }
    } else if (propName == QLatin1String("DesktopFile")) {
        const QString newValue = qvariant_cast<QString>(value);
        if (m_desktopFile != newValue) {
            m_desktopFile = newValue;
            Q_EMIT DesktopFileChanged(newValue);
        }
    } else if (propName == QLatin1String("Icon")) {
        const QString newValue = qvariant_cast<QString>(value);
        if (m_icon != newValue) {
            m_icon = newValue;
            Q_EMIT IconChanged(newValue);
        }
    } else if (propName == QLatin1String("Id")) {
        const QString newValue = qvariant_cast<QString>(value);
        if (m_id != newValue) {
            m_id = newValue;
            Q_EMIT IdChanged(newValue);
        }
    } else if (propName == QLatin1String("IsActive")) {
        const bool newValue = qvariant_cast<bool>(value);
        if (m_isActive != newValue) {
            m_isActive = newValue;
            Q_EMIT IsActiveChanged(newValue);
        }
    } else if (propName == QLatin1String("IsDocked")) {
        const bool newValue = qvariant_cast<bool>(value);
        if (m_isDocked != newValue) {
            m_isDocked = newValue;
            Q_EMIT IsDockedChanged(newValue);
        }
    } else if (propName == QLatin1String("Menu")) {
        const QString newValue = qvariant_cast<QString>(value);
        if (m_menu != newValue) {
            m_menu = newValue;
            Q_EMIT MenuChanged(newValue);
        }
    } else if (propName == QLatin1String("Name")) {
        const QString newValue = qvariant_cast<QString>(value);
        if (m_name != newValue) {
            m_name = newValue;
            Q_EMIT NameChanged(newValue);
        }
    } else if (propName == QLatin1String("WindowInfos")) {
        const WindowInfoMap newValue = qvariant_cast<WindowInfoMap>(value);
        if (m_windowInfos != newValue) {
            m_windowInfos = newValue;
            Q_EMIT WindowInfosChanged(newValue);
        }
    }
}
