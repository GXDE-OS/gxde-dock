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

#include "pluginloader.h"

#include <QDir>
#include <QDebug>
#include <QLibrary>
#include <QSet>

PluginLoader::PluginLoader(const QString &pluginDirPath, QObject *parent)
    : PluginLoader(QStringList() << pluginDirPath, parent)
{
}

PluginLoader::PluginLoader(const QStringList &pluginDirPaths, QObject *parent)
    : QThread(parent)
    , m_pluginDirPaths(pluginDirPaths)
{
}

void PluginLoader::run()
{
    QSet<QString> loadedPluginFiles;

    for (const QString &pluginDirPath : m_pluginDirPaths) {
        QDir pluginsDir(pluginDirPath);
        const QStringList plugins = pluginsDir.entryList(QDir::Files);

        for (const QString &file : plugins) {
            if (!QLibrary::isLibrary(file))
                continue;

            // TODO: old dock plugins is uncompatible
            if (file.startsWith("libgxde-dock-"))
                continue;

            // Prefer the first directory's copy while still loading plugins
            // that are only installed in later directories.
            if (loadedPluginFiles.contains(file))
                continue;

            loadedPluginFiles.insert(file);
            emit pluginFounded(pluginsDir.absoluteFilePath(file));
        }
    }

    emit finished();
}
