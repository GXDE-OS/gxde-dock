
#ifndef APPSPREVIEWPROVIDER_H
#define APPSPREVIEWPROVIDER_H

#include "previewcontainer.h"

static PreviewContainer *PreviewWindow(const WindowInfoMap &infos, const WindowList &allowClose,
        const Dock::Position dockPos, const QString &appId = QString())
{
    static PreviewContainer *preview;
    if (!preview) {
        preview = new PreviewContainer;
    }

    preview->disconnect();
    preview->setWindowInfos(infos, allowClose, appId);
    preview->updateLayoutDirection(dockPos);

    return preview;
}

#endif /* APPSPREVIEWPROVIDER_H */
