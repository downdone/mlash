#pragma once
#include "app/AppController.h"
#include <QDialog>
class QListWidget; class QTabWidget; class QLabel; class QPushButton;
namespace mlash {
class ProxyEditorDialog final : public QDialog {
    Q_OBJECT
public:
    ProxyEditorDialog(AppController* controller,const QString& id,QWidget* parent=nullptr);
private:
    void refresh();
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
    QTabWidget* tabs_; QListWidget *nodes_,*groups_; QLabel* hint_;
    QPushButton *save_,*add_,*edit_,*remove_,*restore_,*reset_,*import_;
};
}
