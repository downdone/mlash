#pragma once

class QComboBox;
namespace mlash {
class WidgetStyle {
public:
    static void install();
    static void prepareCombo(QComboBox* combo);
};
}
