#pragma once
#include "domain/Types.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"
namespace mlash {
class HotkeysDialog final : public TaskDialog {
    Q_OBJECT
public:
    explicit HotkeysDialog(AppController* controller,QWidget* parent=nullptr);
};
}
