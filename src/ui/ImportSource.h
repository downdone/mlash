#pragma once
#include <QString>

namespace mlash {

// 快速导入输入的分类结果：空表示无法识别。
struct ImportSource {
    QString value;
    bool remote = false;
};

// 识别 URL / 本地文件路径；其余交由 isNodeImport 判定。
// 两个函数由配置页与导入任务共用，避免两处各写一份判定。
ImportSource classifyImport(const QString& input);
bool isNodeImport(const QString& value);

} // namespace mlash