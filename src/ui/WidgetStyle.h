#pragma once

class QComboBox;
class QMenu;
class QPoint;
namespace mlash {
class WidgetStyle {
public:
    static void install();
    static void prepareCombo(QComboBox* combo);
    // Measure and clamp before opening, so the first painted popup has its final geometry.
    static void popupMenu(QMenu* menu, const QPoint& globalAnchor, bool alignRight = false);
};
}
