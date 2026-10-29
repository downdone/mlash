#pragma once
#include <QSpinBox>

namespace mlash {
// Same rule as CompactComboBox: a stray wheel over an unfocused number field changes
// what the user is reading instead of scrolling the page. Focus restores the usual
// wheel and keyboard editing.
class CompactSpinBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;
    bool event(QEvent* event) override {
        if (event->type()==QEvent::Wheel && !hasFocus()) { event->ignore(); return false; }
        return QSpinBox::event(event);
    }
};
}
