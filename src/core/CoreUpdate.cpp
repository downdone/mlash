#include "CoreUpdate.h"
#include "storage/Storage.h"
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QtEndian>
#include <QTimer>
namespace mlash {
namespace {
const QRegularExpression digest("^[a-f0-9]{64}$");
QString hash(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
bool managedDirectory(const QString& root,const QString& directory) { const auto base=QFileInfo(root+"/core-updates").canonicalFilePath(); const auto folder=QFileInfo(directory); const auto canonical=folder.canonicalFilePath(); return !base.isEmpty()&&!QFileInfo(root+"/core-updates").isSymLink()&&!folder.isSymLink()&&folder.isDir()&&QFileInfo(canonical).absolutePath().compare(base,Qt::CaseInsensitive)==0&&digest.match(folder.fileName()).hasMatch(); }
bool inside(const QString& root,const QString& path) { const auto file=QFileInfo(path); return file.isFile()&&!file.isSymLink()&&file.fileName()=="mihomo.exe"&&managedDirectory(root,file.absolutePath()); }
}
Result CoreUpdate::stage(ProfileStore& store,const QString& source) {
#ifndef Q_OS_WIN
    Q_UNUSED(store); Q_UNUSED(source); return Result::fail("此平台不支持Windows内核更新");
#else
    const auto file=QFileInfo(source); if (!file.isAbsolute()||!file.isFile()||file.isSymLink()) return Result::fail("请选择本地完整mihomo.exe文件，不能为链接"); QByteArray bytes; auto result=readFile(file.absoluteFilePath(),bytes,128*1024*1024); if (!result.ok) return result;
    return stageBytes(store,bytes);
#endif
}
Result CoreUpdate::stageBytes(ProfileStore& store,const QByteArray& bytes) {
#ifndef Q_OS_WIN
    Q_UNUSED(store); Q_UNUSED(bytes); return Result::fail("此平台不支持Windows内核更新");
#else
    if (bytes.size()>128*1024*1024) return Result::fail("内核程序超过128 MiB");
    Result result;
    if (bytes.size()<64||bytes.left(2)!="MZ") return Result::fail("内核文件不是Windows PE程序"); const auto offset=qFromLittleEndian<quint32>(bytes.constData()+0x3c); if (offset>quint32(bytes.size()-6)||bytes.mid(offset,4)!=QByteArray("PE\0\0",4)||qFromLittleEndian<quint16>(bytes.constData()+offset+4)!=0x8664) return Result::fail("内核文件不是Windows x64程序");
    const auto sha=hash(bytes),base=store.root()+"/core-updates",directory=base+'/'+sha,path=directory+"/mihomo.exe"; if (QFileInfo(base).isSymLink()||QFileInfo(directory).isSymLink()) return Result::fail("内核存储目录不能为链接"); result=store.createPrivateDirectory(base); if (!result.ok) return result;
    const QJsonObject record{{"sha256",sha},{"bytes",double(bytes.size())},{"path",path}}; if (QFileInfo::exists(directory)) { const auto verified=verify(store.root(),record); return verified.ok?Result{true,{},record}:verified; }
    if (QDir(base).entryList(QDir::Dirs|QDir::NoDotAndDotDot).size()>=8) return Result::fail("最多保留8个内核副本，请先清理未使用版本"); result=store.createPrivateDirectory(directory); if (!result.ok) return result; result=writeAtomic(path,bytes); if (result.ok) result=writeAtomic(directory+"/managed-core.json",QJsonDocument(record).toJson(QJsonDocument::Compact)); if (!result.ok) { if (managedDirectory(store.root(),directory)) QDir(QFileInfo(directory).canonicalFilePath()).removeRecursively(); return result; } return {true,{},record};
#endif
}
Result CoreUpdate::verify(const QString& root,const QJsonObject& record) {
    const auto path=record.value("path").toString(),sha=record.value("sha256").toString(); if (!inside(root,path)||!digest.match(sha).hasMatch()||QFileInfo(QFileInfo(path).absolutePath()).fileName()!=sha) return Result::fail("内核副本路径或哈希无效"); QByteArray bytes,metadata; auto result=readFile(path,bytes,128*1024*1024); if (!result.ok||hash(bytes)!=sha||double(bytes.size())!=record.value("bytes").toDouble(-1)) return Result::fail("内核副本自预检后发生变化，未执行"); result=readFile(QFileInfo(path).absolutePath()+"/managed-core.json",metadata,4096); if (!result.ok||QJsonDocument::fromJson(metadata).object()!=QJsonObject{{"sha256",sha},{"bytes",double(bytes.size())},{"path",path}}) return Result::fail("内核副本清单不一致"); return {};
}
Result CoreUpdate::clean(ProfileStore& store,const QStringList& keep) {
    const auto base=store.root()+"/core-updates"; if (QFileInfo(base).isSymLink()) return Result::fail("内核目录为链接，未清理"); int count=0;
    for (const auto& entry:QDir(base).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot)) { if (entry.isSymLink()||!digest.match(entry.fileName()).hasMatch()) continue; const auto path=entry.absoluteFilePath()+"/mihomo.exe"; bool retained=false; for (const auto& saved:keep) if (QFileInfo(saved).absoluteFilePath().compare(path,Qt::CaseInsensitive)==0) retained=true; if (retained) continue; const auto files=QDir(entry.absoluteFilePath()).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden); if (files.size()!=2||!files.contains("mihomo.exe")||!files.contains("managed-core.json")) continue; QByteArray metadata; if (!readFile(entry.absoluteFilePath()+"/managed-core.json",metadata,4096).ok) continue; const auto record=QJsonDocument::fromJson(metadata).object(); if (record.size()!=3||record.value("path").toString()!=path||record.value("sha256").toString()!=entry.fileName()||!inside(store.root(),path)) continue; if (!QDir(entry.canonicalFilePath()).removeRecursively()) return Result::fail("未使用内核副本清理失败"); ++count; }
    return {true,{},{{"removed",count}}};
}
void CoreVersionProbe::cancel() { if (process_) { process_->setProperty("cancelled",true); process_->kill(); } }
void CoreVersionProbe::inspect(const QString& path,Completion done) {
    if (process_) { done(Result::fail("内核版本预检正在进行")); return; } auto* process=new QProcess(this); process_=process; process->setProgram(path); process->setArguments({"-v"}); process->setWorkingDirectory(QFileInfo(path).absolutePath()); auto env=QProcessEnvironment::systemEnvironment(); for (const auto& key:env.keys()) if (key.startsWith("CLASH_")||key=="SAFE_PATHS"||key=="SKIP_SAFE_PATH_CHECK") env.remove(key); env.insert("SKIP_SAFE_PATH_CHECK","false"); process->setProcessEnvironment(env); auto* timer=new QTimer(process); timer->setSingleShot(true); auto completion=std::make_shared<Completion>(std::move(done)); connect(timer,&QTimer::timeout,process,[process] { process->setProperty("timedOut",true); process->kill(); });
    connect(process,&QProcess::errorOccurred,this,[this,process,completion](QProcess::ProcessError error) { if (error!=QProcess::FailedToStart) return; process_=nullptr; auto done=std::move(*completion); process->deleteLater(); if (done) done(Result::fail("内核版本预检程序无法启动")); });
    const auto collect=[process] { auto bytes=process->property("output").toByteArray(); bytes+=process->readAllStandardOutput(); bytes+=process->readAllStandardError(); if (bytes.size()>8192) { bytes.truncate(8192); process->setProperty("oversize",true); process->kill(); } process->setProperty("output",bytes); }; connect(process,&QProcess::readyReadStandardOutput,process,collect); connect(process,&QProcess::readyReadStandardError,process,collect);
    connect(process,&QProcess::finished,this,[this,process,completion,collect](int code,QProcess::ExitStatus status) { collect(); process_=nullptr; const auto output=QString::fromUtf8(process->property("output").toByteArray()); static const QRegularExpression version("^Mihomo Meta (v[0-9]+\\.[0-9]+\\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?) windows amd64(?: |$)"); const auto match=version.match(output); Result result; if (process->property("cancelled").toBool()) result=Result::fail("内核预检已取消"); else if (process->property("timedOut").toBool()||process->property("oversize").toBool()||code!=0||status!=QProcess::NormalExit||!match.hasMatch()) result=Result::fail("内核预检未返回受支持的Windows x64 Mihomo版本（3秒/8KiB限制）"); else result.data={{"version",match.captured(1)}}; auto done=std::move(*completion); process->deleteLater(); if (done) done(result); }); timer->start(3000); process->start();
}
}
