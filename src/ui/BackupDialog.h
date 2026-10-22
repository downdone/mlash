#pragma once
#include "app/AppController.h"
#include <QDialog>

class QListWidget; class QLineEdit; class QCheckBox; class QLabel; class QPushButton; class QPlainTextEdit;
namespace mlash {
class BackupDialog final : public QDialog {
    Q_OBJECT
public:
    BackupDialog(AppController* controller, bool restore, QWidget* parent = nullptr);
    void setBackupPath(const QString& path);
private:
    void updateActions();
    AppController* controller_;
    bool restore_, previewing_ = false, submitted_ = false;
    bool stopping_ = false;
    QString digest_;
    QLineEdit *path_, *password_, *confirm_;
    QCheckBox *profiles_, *settings_;
    QLabel* hint_;
    QLabel* contents_;
    QListWidget* selection_;
    QStringList previewNames_;
    QPushButton *selectAll_, *selectNone_;
    QPushButton *preview_, *submit_;
    QPushButton* stopCore_;
};
}
