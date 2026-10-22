#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QSet>
class QLineEdit; class QComboBox; class QCheckBox; class QLabel; class QPushButton;
namespace mlash {
class ProviderDefinitionDialog final : public QDialog {
public:
    ProviderDefinitionDialog(const QString& name,const QJsonObject& definition,const QSet<QString>& occupied,const QStringList& targets,bool canAddRule,QWidget* parent=nullptr);
    QString providerName() const;
    QJsonObject definition() const;
    bool addReference() const;
    QString target() const;
private:
    void validate();
    QLineEdit *name_,*url_; QComboBox *behavior_,*format_,*target_; QCheckBox* reference_; QLabel* hint_; QPushButton* save_;
    QSet<QString> occupied_; bool adding_;
};
}
