#pragma once
#include "DialogDraftGuard.h"
#include "TaskWorkspace.h"
#include <QDialog>
namespace mlash {
// A reusable form can run standalone in tests or inside the main workspace.
class TaskDialog : public QDialog {
public:
    using QDialog::QDialog;
    void reject() override {
        if (property("guardReject").toBool() && !DialogDraftGuard::allowLeave(this)) return;
        if (property("embeddedTask").toBool()) {
            for (auto* parent=parentWidget();parent;parent=parent->parentWidget()) if (auto* workspace=dynamic_cast<TaskWorkspace*>(parent)) { if (!workspace->canClose(this)) return; break; }
            if (!DialogDraftGuard::allowLeave(this)) return;
        }
        QDialog::reject();
    }
};
}
