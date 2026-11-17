#pragma once
#include "core/CoreApi.h"
#include <QDateTime>
#include <QPointer>

namespace mlash {
// 首页控制区第三行「连通性」的唯一实现（R3b）。
// 与 WebsiteTests 的区别：它不受测速页可见性约束，只做一次只读延迟请求，
// 结果自带头部状态与时间，供首页就地显示。不改变节点选择、不周期轮询。
class ConnectivityProbe final : public QObject {
    Q_OBJECT
public:
    explicit ConnectivityProbe(CoreApi* api, QObject* parent = nullptr);
    ~ConnectivityProbe() override;

    // 目标地址：默认沿用设置里的测试地址；为空时回退到 WebsiteTests 的首个目标。
    void setUrl(const QString& url);
    QString url() const { return url_; }

    // 发起一次探测。node 必须是可解析的叶子（AppController 负责解析）；为空则拒绝。
    void run(const QString& node);
    void cancel();

    bool running() const { return request_ != nullptr; }
    // {"status":"untested|running|ok|failed","delay":int,"node":str,"url":str,"measured_utc":str,"error":str}
    const QJsonObject& result() const { return result_; }
    // 结果是否已经超出可使用期限（默认 10 分钟）；过期的结果仍保留，但界面必须标注过期。
    bool stale() const;

signals:
    void changed();

private:
    void finish(const QJsonObject& result);
    CoreApi* api_;
    QPointer<QNetworkReply> request_;
    QString url_, node_;
    quint64 generation_ = 0;
    QJsonObject result_;
};
} // namespace mlash
