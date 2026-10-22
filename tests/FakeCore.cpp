#include <yaml-cpp/yaml.h>
#include "MockHttp.h"
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdlib>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    if (args.contains("-v")) {
#ifdef MLASH_UPDATE_FIXTURE
        printf("Mihomo Meta v9.99.0 windows amd64 test fixture\n"); fflush(stdout); if (qEnvironmentVariableIsSet("MLASH_TEST_CORE_VERSION_SLOW")) { QTimer::singleShot(5000,&app,&QCoreApplication::quit); return app.exec(); }
#else
        printf("Fake Core (test only)\n");
#endif
        return 0;
    }
    const int index = args.indexOf("-f"); if (index < 0 || index + 1 >= args.size()) return 2;
    QFile file(args[index + 1]); if (!file.open(QIODevice::ReadOnly)) return 2; const auto text = QString::fromUtf8(file.readAll());
    if (args.contains("-t")) {
        if (text.contains("fake-slow-validation: true")) { QTimer::singleShot(21000, &app, &QCoreApplication::quit); return app.exec(); }
        return text.contains("invalid-core: true") ? 1 : 0;
    }
    const int mixed = QRegularExpression("mixed-port: ([0-9]+)").match(text).captured(1).toInt();
    const int api = QRegularExpression("external-controller: ['\"]?127.0.0.1:([0-9]+)").match(text).captured(1).toInt();
    const auto secret = QRegularExpression("secret: ['\"]?([a-zA-Z0-9]+)").match(text).captured(1).toLatin1();
    const bool tunEnabled=QRegularExpression("tun:\\s*\\n\\s*enable: true").match(text).hasMatch();
    QJsonObject tun{{"enable",tunEnabled},{"device","Mlash"},{"auto-route",true},{"auto-detect-interface",true},{"strict-route",text.contains("strict-route: true")},{"stack",qEnvironmentVariableIsSet("MLASH_TEST_TUN_BAD_READBACK")?QString("incorrect"):QRegularExpression("stack: ([a-z]+)").match(text).captured(1)}};
    try { const auto config=YAML::Load(text.toStdString()); const auto options=config["tun"]; QJsonArray excludes,hijack; if (options&&options.IsMap()) { if (options["route-exclude-address"]) for (const auto& route:options["route-exclude-address"]) excludes.append(QString::fromStdString(route.as<std::string>())); if (options["dns-hijack"]) for (const auto& value:options["dns-hijack"]) hijack.append(QString::fromStdString(value.as<std::string>())); tun["mtu"]=options["mtu"].as<int>(1500); } else tun["mtu"]=1500; tun["route-exclude-address"]=excludes; tun["dns-hijack"]=hijack; } catch (...) {}

    MockHttp http; QTcpServer proxy; if (!http.listen(QHostAddress::LocalHost, api) || !proxy.listen(QHostAddress::LocalHost, mixed)) return 3;
    QObject::connect(&proxy, &QTcpServer::newConnection, &proxy, [&proxy] { while (proxy.hasPendingConnections()) { auto* socket = proxy.nextPendingConnection(); socket->disconnectFromHost(); socket->deleteLater(); } });
    QString mode = "rule", selection = "DIRECT";
    http.handler = [&](const QByteArray& request) {
        if (!request.contains("Bearer " + secret)) return HttpResponse{401, "{\"message\":\"Unauthorized\"}"};
        const auto first = request.left(request.indexOf("\r\n")); const auto path = first.split(' ').value(1);
        const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
        if (path == "/version") return HttpResponse{200, "{\"version\":\"fake-test\",\"meta\":true}"};
        if (path == "/rules") return HttpResponse{200,"{\"rules\":[{\"type\":\"Match\",\"payload\":\"\",\"proxy\":\"Test\"}]}"};
        if (path == "/configs") {
            if (first.startsWith("PATCH")) { mode = body.value("mode").toString(); return HttpResponse{204, {}}; }
            return HttpResponse{200, QJsonDocument(QJsonObject{{"mode", mode}, {"mixed-port", mixed},{"tun",tun}}).toJson()};
        }
        const QJsonObject group{{"type", "Selector"}, {"now", selection}, {"all", QJsonArray{"DIRECT", "REJECT"}}};
        if (path == "/proxies") return HttpResponse{200, QJsonDocument(QJsonObject{{"proxies", QJsonObject{{"Test", group}, {"DIRECT", QJsonObject{{"type", "Direct"}}}, {"REJECT", QJsonObject{{"type", "Reject"}}}}}}).toJson()};
        if (path.startsWith("/proxies/Test")) {
            if (first.startsWith("PUT")) { selection = body.value("name").toString(); return HttpResponse{204, {}}; }
            return HttpResponse{200, QJsonDocument(group).toJson()};
        }
        return HttpResponse{404, "{\"message\":\"not found\"}"};
    };
    // Inject an unexpected nonzero exit without invoking the host's crash reporter.
    if (text.contains("fake-crash: true")) QTimer::singleShot(800, &app, [] { std::_Exit(17); });
    return app.exec();
}
