#pragma once
#include "app/AppController.h"
#include <QDialog>
namespace mlash {
class HomeSummaryDialog final : public QDialog {
    Q_OBJECT
public:
    explicit HomeSummaryDialog(AppController* controller,QWidget* parent=nullptr);
};
}
