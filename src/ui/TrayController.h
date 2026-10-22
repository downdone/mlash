#pragma once
#include "app/AppController.h"
#include <QSystemTrayIcon>
#include <QMenu>

namespace mlash {
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(AppController* controller, QObject* parent = nullptr);
    ~TrayController() override;
    bool available() const { return available_; }
    static QIcon icon();
    static QString appearance(const AppState& state);
    static QIcon statusIcon(const AppState& state);
    static QString statusTooltip(const AppState& state);
    QString visualKey() const { return visualKey_; }
    qint64 measureMenuPreparation();
signals:
    void exitRequested();
    void showWindow();
    void showPage(int index);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void rebuildMenu();
    void updateState();
    void updateGroupMenu();
    AppController* controller_;
    QSystemTrayIcon tray_;
    QMenu menu_;
    bool available_;
    QString visualKey_;
    QTimer clickTimer_;
    QTimer groupDwell_;
    QPointer<QMenu> activeGroupMenu_;
    bool autoTestGroups_ = true;
    QHash<QString, QIcon> menuIcons_;
};
}
