#pragma once
#include "domain/Types.h"
namespace mlash { class AppController; }
#include "TaskDialog.h"
class QListWidget; class QTabWidget; class QLabel; class QPushButton; class QLineEdit; class QAction;
namespace mlash {
class ProxyEditorDialog final : public TaskDialog {
    Q_OBJECT
public:
    ProxyEditorDialog(AppController* controller,const QString& id,QWidget* parent=nullptr);
private:
    void loadEntries();
    void refresh();
    void filterEntries();
    void updateActions();
    void edit(bool create);
    void previewRemoval();
    void importLinks();
    bool sourceCurrent() const;
    void change(const QString& key,const QJsonValue& value);
    QString key() const;
    QListWidget* list() const;
    AppController* controller_; QString id_,editVersion_; QJsonObject saved_,draft_,source_,sourceExpected_;
    bool loaded_=false,saving_=false,analyzing_=false;
    QTabWidget* tabs_; QListWidget *nodes_,*groups_; QLabel* hint_; QLineEdit* search_;
    QPushButton *save_,*add_,*edit_,*retry_; QAction *remove_,*restore_,*reset_,*import_;
};
}
