#pragma once
// CONTRACT owner=storage layer=storage
// invariant: writeAtomic 先写临时文件再原子替换；readFile 默认上限 8 MiB
// forbidden: 不得在 GUI 线程做大文件读写；不得绕过 DPAPI 写明文索引
// lifecycle: 只由有界 worker 调用，失败返回 Result 而不抛出
// tests: backup_tests
// spec: docs/architecture.md
#include "domain/Types.h"
#include <QByteArray>

namespace mlash {
Result normalizeProxyBypass(const QString& text);
bool validTestUrl(const QString& text);
Result validateNodePreferences(const QJsonObject& object);
Result validateConnectionView(const QJsonObject& object);
bool validAppearance(const QString& appearance,int scale);
Result writeAtomic(const QString& path, const QByteArray& data);
Result readFile(const QString& path, QByteArray& data, qint64 limit = 8 * 1024 * 1024);
QString newId();
bool isId(const QString& id);
}
