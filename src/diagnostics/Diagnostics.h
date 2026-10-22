#pragma once
#include <QObject>
#include <QThread>
#include <QStringList>
#include <atomic>
#include <memory>

namespace mlash {
class Diagnostics : public QObject {
    Q_OBJECT
public:
    explicit Diagnostics(QString root, QObject* parent = nullptr);
    ~Diagnostics() override;
    void append(const QString& source, const QString& message);
    void setSecrets(const QStringList& secrets);
    QStringList lines() const { return lines_; }
    static QString redact(QString text);
    QString sanitize(QString text) const;
    static QString sanitize(QString text, const QStringList& secrets);
    struct Snapshot { QStringList lines, secrets; };
    Snapshot snapshot() const { return {lines_, secrets_}; }
    static QString levelOf(const QString& line);
    void clearView();
signals:
    void lineAdded(const QString& line);
    void viewCleared();
private:
    QStringList lines_, secrets_;
    QString logPath_;
    QThread thread_;
    QObject* writer_;
    std::shared_ptr<std::atomic<int>> pending_ = std::make_shared<std::atomic<int>>(0);
};
}
