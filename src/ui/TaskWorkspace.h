#pragma once
#include <QWidget>
#include <QPointer>
#include <QList>
#include <functional>
class QTabBar; class QStackedWidget; class QLabel; class QPushButton; class QDialog; class QHBoxLayout;
namespace mlash {
// One bounded edit workspace in the main content area. Tabs retain their drafts.
class TaskWorkspace final : public QWidget {
    Q_OBJECT
public:
    struct Task { QString key, title; std::function<void()> open; };
    explicit TaskWorkspace(QWidget* parent=nullptr);
    ~TaskWorkspace() override;
    bool configure(const QString& context, const QString& title, const QList<Task>& tasks,
                   std::function<void()> back);
    QWidget* editor(const QString& key) const;
    void mount(QWidget* editor, const QString& key={}, const QString& title={});
    void select(const QString& key);
    QString context() const { return context_; }
    bool creating() const { return !creatingKey_.isEmpty(); }
    bool canReplace();
    bool canClose(QWidget* editor);
    void clear();
    void note(const QString& text);
    std::function<void()> requested;
private:
    struct Entry { Task task; QPointer<QWidget> editor, surface, owner; };
    void activate(int index);
    void updateHeading(const QString& taskTitle);
    void removeEditor(const QString& key);
    int indexOf(const QString& key) const;
    QString context_, contextTitle_, creatingKey_;
    QList<Entry> entries_;
    QTabBar* tabs_;
    QStackedWidget* panels_;
    QLabel *title_, *hint_;
    QPushButton* back_;
    QHBoxLayout* header_;
    std::function<void()> backAction_;
    bool clearing_=false;
};
void presentTaskDialog(QDialog* dialog);
}
