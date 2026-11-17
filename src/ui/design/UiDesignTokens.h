#pragma once

namespace mlash::design {

// Semantic colors describe component roles, independent of Qt's runtime palette.
struct Colors {
    const char *window, *sidebar, *surface;
    const char *ink, *secondary, *muted, *disabled;
    const char *border, *separator, *brand, *selection;
    const char *primary, *onPrimary, *hover, *focus;
    const char *control, *pressed, *primaryPressed;
    const char *success, *warning, *error, *track, *artworkSurface;
};

// Physical dimensions are in device-independent pixels. Text scaling is applied
// to fonts and content-sensitive rows by the Qt adapter, not to every margin.
struct Geometry {
    int controlHeight, tableRow, panelGap, popupRadius;
    int contentInset, contentTop, panelInset, sidebarWidth;
    int topbarHeight, sidebarItemHeight, rowHeight, nodeRowHeight;
    int settingRowHeight, profileRowHeight, nodeCardHeight, drawerWidth;
    int sidebarInset, sidebarGap, topbarHorizontalInset, topbarActionSlotWidth, topbarActionGap;
};

struct Typography {
    double bodyPointsWindows, bodyPointsMac, captionRatio, captionFloor;
    int dialogPoints, sectionPoints, valuePoints, titlePoints;
};

} // namespace mlash::design
