#pragma once
#include "TaskDialog.h"
#include "domain/Types.h"
namespace mlash { class AppController; }
namespace mlash {
class ProviderPolicyDialog final : public TaskDialog {
public:
    ProviderPolicyDialog(AppController* controller,const QString& profileId,const QJsonObject& provider,QWidget* parent=nullptr);
    void reject() override;
private:
    std::shared_ptr<bool> pending_=std::make_shared<bool>(false);
};
}
