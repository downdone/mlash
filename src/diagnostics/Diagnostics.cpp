#include "Diagnostics.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

namespace mlash {
Diagnostics::Diagnostics(QString root, QObject* parent) : QObject(parent), logPath_(root + "/logs/app.log"), writer_(new QObject) {
    writer_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, writer_, &QObject::deleteLater);
    thread_.start();
}
Diagnostics::~Diagnostics() { thread_.quit(); thread_.wait(); }
QString Diagnostics::redact(QString text) {
    static const QRegularExpression urlCredentials(R"((https?://)[^\s/@]+:[^\s/@]+@)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression query(R"((https?://[^\s?"'<>]+)\?[^\s"'<>]*)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression authorization(R"((\bauthorization\b["']?\s*[:=]\s*["']?(?:Bearer|Basic)\s+)[^\s"'}]+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression fields(R"((\b(?:password|passwd|secret|token|authorization|uuid|private-key|api-key)\b["']?\s*[:=]\s*)[^\s,;}]+)", QRegularExpression::CaseInsensitiveOption);
    text.replace(urlCredentials, "\\1[redacted]@"); text.replace(query, "\\1?[redacted]"); text.replace(authorization, "\\1[redacted]"); text.replace(fields, "\\1[redacted]");
    return text.left(4096);
}
void Diagnostics::setSecrets(const QStringList& secrets) {
    secrets_.append(secrets); secrets_.removeDuplicates();
    std::sort(secrets_.begin(), secrets_.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    if (secrets_.size() > 10000) secrets_ = secrets_.mid(0, 10000);
}
QString Diagnostics::sanitize(QString text) const {
    return sanitize(std::move(text), secrets_);
}
QString Diagnostics::sanitize(QString text, const QStringList& secrets) {
    for (const auto& secret : secrets) if (secret.size() >= 4) text.replace(secret, "[redacted]");
    return redact(text).replace('\r', ' ').replace('\n', ' ');
}
QString Diagnostics::levelOf(const QString& line) {
    if (line.contains("[error]")) return "error";
    if (line.contains("[app]")) return "app";
    static const QRegularExpression level(R"((?:\blevel=|\[core-log\]\s+)(debug|info|warning|error|fatal)\b)", QRegularExpression::CaseInsensitiveOption);
    const auto match = level.match(line); const auto value = match.captured(1).toLower();
    return value == "fatal" ? "error" : value;
}
void Diagnostics::clearView() { lines_.clear(); emit viewCleared(); }
void Diagnostics::append(const QString& source, const QString& message) {
    const auto filtered = sanitize(message);
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + " [" + source + "] " + filtered;
    lines_.append(line); if (lines_.size() > 2000) lines_.removeFirst();
    emit lineAdded(line);
    const auto path = logPath_;
    const auto pending = pending_;
    if (pending->fetch_add(1) >= 512) { pending->fetch_sub(1); return; }
    QMetaObject::invokeMethod(writer_, [path, line, pending] {
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (QFileInfo(path).size() >= 5 * 1024 * 1024) {
            QFile::remove(path + ".4");
            for (int i = 3; i >= 1; --i) QFile::rename(path + "." + QString::number(i), path + "." + QString::number(i + 1));
            QFile::rename(path, path + ".1");
        }
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) { file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); file.write(line.toUtf8() + '\n'); }
        pending->fetch_sub(1);
    }, Qt::QueuedConnection);
}
}
