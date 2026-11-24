#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace mlash {

class ProxyNodesPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProxyNodesPage(QWidget* parent = nullptr);
    void attachNodes(QWidget* nodes);
    void setAvailability(bool hasActive, bool hasProfiles, bool canChooseProfile);

signals:
    void profileSetupRequested();

private:
    QWidget* empty_;
    QLabel* title_;
    QLabel* hint_;
    QPushButton* setup_;
    QWidget* nodesHost_;
    QVBoxLayout* nodesLayout_;
};

} // namespace mlash
