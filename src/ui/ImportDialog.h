#pragma once
#include <QDialog>
#include "app/Types.h"

class QTabWidget; class QLineEdit; class QLabel; class QPushButton;
namespace mlash {
class DownloadPolicyEditor;
class NodeLinksEditor;
class AppController;
class ImportDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ImportDialog(QWidget* parent = nullptr);
    bool isUrl() const;
    bool isNodeLinks() const;
    QString source() const;
    QString profileName() const;
    DownloadPolicy downloadPolicy() const;
    void setSourceTab(bool url);
    void setPrefill(const QString& source,bool url);
    void setNodeLinks(const QString& text);
    void setProtocolSource(const QString& url,const QString& name);
    void setImportAvailable(bool available) { available_=available; validateInput(); }
    void bindController(AppController* controller);
    void accept() override;
private:
    void validateInput();
    void inputChanged() { failure_.clear(); validateInput(); }
    QTabWidget* tabs_;
    QLineEdit *url_, *file_, *name_;
    QLabel* hint_;
    QPushButton* import_;
    DownloadPolicyEditor* download_;
    NodeLinksEditor* links_;
    bool available_=true;
    bool pending_=false;
    QString failure_;
    AppController* controller_=nullptr;
};
}
