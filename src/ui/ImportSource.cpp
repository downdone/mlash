#include "ImportSource.h"
#include "ImportDialog.h"
#include "profile/NodeLinks.h"
#include <QFileInfo>
#include <QUrl>

namespace mlash {

ImportSource classifyImport(const QString& input) {
    auto text = input.trimmed();
    if (text.size() > 4096 || text.contains('\n') || text.contains('\r')) return {};
    if (text.size() > 1 && text.startsWith('"') && text.endsWith('"')) text = text.mid(1, text.size() - 2);
    const QUrl url(text);
    if (ImportDialog::acceptsUrl(url)) return {text, true};
    const auto path = url.isLocalFile() ? url.toLocalFile() : text;
    if (QFileInfo(path).isAbsolute() &&
        QStringList{"yaml", "yml", "conf"}.contains(QFileInfo(path).suffix().toLower()))
        return {path, false};
    return {};
}

bool isNodeImport(const QString& value) {
    const auto scheme = value.section(':', 0, 0).toLower();
    return value.size() <= 64 * 1024 && scheme != "http" && scheme != "https" &&
           NodeLinks::acceptsScheme(scheme);
}

} // namespace mlash