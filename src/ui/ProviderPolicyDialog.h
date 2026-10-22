#pragma once
#include <QDialog>
#include "app/AppController.h"
namespace mlash {
class ProviderPolicyDialog final : public QDialog {
public:
    ProviderPolicyDialog(AppController* controller,const QString& profileId,const QJsonObject& provider,QWidget* parent=nullptr);
    void reject() override;
private:
    std::shared_ptr<bool> pending_=std::make_shared<bool>(false);
};
}
