#pragma once
#include "app/AppController.h"
#include <QDialog>
namespace mlash {
class HotkeysDialog final : public QDialog {
    Q_OBJECT
public:
    explicit HotkeysDialog(AppController* controller,QWidget* parent=nullptr);
};
}
