#include "ServiceDiagnostics.h"
#include <QJsonDocument>
#include <QRegularExpression>
namespace mlash {
QList<ServiceTarget> serviceTargets() { return {{"chatgpt","ChatGPT",QUrl("https://api.openai.com/compliance/cookie_requirements")},{"youtube","YouTube Premium",QUrl("https://www.youtube.com/premium?hl=en")},{"netflix","Netflix",QUrl("https://www.netflix.com/title/70143836")},{"claude","Claude",QUrl("https://claude.ai/cdn-cgi/trace")},{"gemini","Gemini",QUrl("https://gemini.google.com/")}}; }
QJsonObject analyzeService(const QString& id,int status,const QByteArray& body,const QUrl& redirect) {
    QJsonObject result{{"state","unknown"},{"evidence",QString("HTTP %1，未识别到足够的服务特征").arg(status)}};
    if (body.size()>2*1024*1024) { result["evidence"]="响应超过诊断上限"; return result; }
    const auto text=QString::fromUtf8(body), lower=text.toLower();
    if (lower.contains("cf-chl-")||lower.contains("captcha")||lower.contains("verify you are human")) { result["evidence"]="遇到人机验证，无法判断服务可用性"; return result; }
    const auto blocked=[&](const QString& evidence) { result["state"]="restricted"; result["evidence"]=evidence; };
    const auto supported=[&](const QString& evidence) { result["state"]="supported"; result["evidence"]=evidence+"；启发式判断，未验证登录/账号或实际播放"; };
    if (id=="chatgpt") {
        const auto object=QJsonDocument::fromJson(body).object(); const auto code=object.value("error").toObject().value("code").toString();
        if (code.startsWith("unsupported_country")) blocked("OpenAI接口明确返回不支持的国家/地区错误码");
        else if (status==200&&object.contains("cookie_policy")&&!object.value("cookie_policy").isNull()) supported("地区合规端点返回预期cookie_policy字段");
        const auto country=object.value("country").toString().toUpper(); if (QRegularExpression("^[A-Z]{2}$").match(country).hasMatch()) result["region"]=country;
    } else if (id=="youtube") {
        if (lower.contains("premium is not available in your country")||lower.contains("premium is not available in your region")) blocked("Premium页面明确提示地区不可用");
        else if (status==200&&QRegularExpression("\"browseid\"\\s*:\\s*\"spunlimited\"").match(lower).hasMatch()&&lower.contains("ad-free")) supported("Premium订阅页同时出现SPunlimited与ad-free标记");
        const auto match=QRegularExpression("\"(?:gl|countrycode|country_code)\"\\s*:\\s*\"([a-z]{2})\"").match(lower); if (match.hasMatch()) result["region"]=match.captured(1).toUpper();
    } else if (id=="netflix") {
        if (lower.contains("not available in your country")||lower.contains("not available in your region")) blocked("Netflix页面明确提示地区不可用");
        else if (lower.contains("m7111-5059")||lower.contains("you seem to be using an unblocker or proxy")) blocked("Netflix页面提示代理/解锁工具限制");
        else if (status==200&&QRegularExpression("\"isplayable\"\\s*:\\s*true").match(lower).hasMatch()) supported("目标内容返回isPlayable标记");
    } else if (id=="claude") {
        if (redirect.host()=="claude.ai"&&redirect.path().contains("unsupported-country")) blocked("Claude重定向到不支持地区页面");
        else if (lower.contains("claude is not available in your region")||lower.contains("claude is not available in your country")) blocked("Claude页面明确提示地区不可用");
        const auto match=QRegularExpression("(?:^|\\n)loc=([A-Z]{2})(?:\\r?\\n|$)").match(text); if (match.hasMatch()) { result["region"]=match.captured(1); if (result.value("state")=="unknown") result["evidence"]="仅获得边缘节点地区；未据此推断Claude支持地区"; }
    } else if (id=="gemini") {
        if (lower.contains("gemini isn't currently supported in your country")||lower.contains("gemini is not available in your country")) blocked("Gemini页面明确提示地区不可用");
        else if (status==200&&lower.contains("bard-sidenav")&&lower.contains("gemini")) supported("观察到Gemini应用外壳标记");
    }
    return result;
}
}
