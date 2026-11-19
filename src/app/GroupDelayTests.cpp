#include "GroupDelayTests.h"
#include "diagnostics/Diagnostics.h"
#include <QNetworkReply>
#include <QTimer>

namespace mlash {
void GroupDelayTests::start(QString group,QStringList members,QString url) {
    cancel();
    members.removeAll(QString()); members.removeDuplicates();
    queue_=members.mid(0,500); total_=queue_.size(); completed_=0;
    group_=std::move(group); url_=std::move(url); cancelled_=false;
    emit changed(); dispatch();
}
void GroupDelayTests::cancel() {
    const bool wasRunning=running();
    ++generation_; queue_.clear(); const auto requests=requests_; requests_.clear();
    if (wasRunning) cancelled_=true;
    for (const auto& reply:requests) if (reply) reply->abort();
    if (wasRunning) emit changed();
}
void GroupDelayTests::dispatch() {
    const auto token=generation_;
    while (requests_.size()<12 && !queue_.isEmpty()) {
        const auto member=queue_.takeFirst(); requests_.insert(member,{});
        auto* reply=api_->delay(member,[this,member,token,guard=QPointer<GroupDelayTests>(this)](Result response) {
            if (!guard || token!=generation_) return;
            requests_.remove(member); ++completed_;
            const auto value=response.data.value("delay");
            const int delay=response.ok && value.isDouble() && value.toInt(-1)>0 ? value.toInt() : -1;
            const auto error=delay>0?QString():Diagnostics::redact(response.error.isEmpty()?QString("测速失败"):response.error).left(512);
            emit result(member,delay,error,url_); emit changed();
            QTimer::singleShot(0,this,[this,token] { if (token==generation_) dispatch(); });
        },5000,url_);
        if (token==generation_ && requests_.contains(member)) requests_[member]=reply;
        if (token!=generation_) return;
    }
    emit changed();
}
}
