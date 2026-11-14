#pragma once
#include "app/AppController.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include "Route.h"

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
    static QString statusTooltip(const AppState& state, const Settings& settings, const QString& profileName = QString());
    QString activeProfileName() const;
    QString visualKey() const { return visualKey_; }
    qint64 measureMenuPreparation();
signals:
    void exitRequested();
    void showWindow();
    void showPage(int index);
    void showRoute(Route route);
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
