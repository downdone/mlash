#include "OfficialCoreSource.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QtEndian>
#ifdef Q_OS_WIN
#include <QtZlib/zlib.h>
#else
#include <zlib.h>
#endif

namespace mlash {
namespace {
QString hash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
constexpr qint64 ArchiveLimit=64*1024*1024, ExecutableLimit=128*1024*1024;
}
OfficialCoreSource::OfficialCoreSource(QObject* parent,std::unique_ptr<SubscriptionDownloader> downloader)
    :QObject(parent),downloader_(downloader?std::move(downloader):std::make_unique<SubscriptionDownloader>()) {}
Result OfficialCoreSource::parseRelease(const QByteArray& bytes) {
    if (bytes.size()>2*1024*1024) return Result::fail("官方版本元数据超过2 MiB");
    const auto document=QJsonDocument::fromJson(bytes); const auto root=document.object();
    static const QRegularExpression version("^v[0-9]{1,4}\\.[0-9]{1,4}\\.[0-9]{1,4}$"),digest("^sha256:[a-f0-9]{64}$");
    const auto tag=root.value("tag_name").toString();
    if (!document.isObject() || !version.match(tag).hasMatch() || !root.value("draft").isBool() || root.value("draft").toBool() ||
        !root.value("prerelease").isBool() || root.value("prerelease").toBool() || !root.value("assets").isArray() || root.value("assets").toArray().size()>512)
        return Result::fail("没有可验证的官方稳定版本；可能是限流、离线或版本数据异常");
    const auto expected="mihomo-windows-amd64-v1-"+tag+".zip";
    QJsonObject selected;
    for (const auto& item:root.value("assets").toArray()) {
        const auto asset=item.toObject(); if (asset.value("name")!=expected) continue;
        if (!selected.isEmpty()) return Result::fail("官方版本出现重复Windows x64资产");
        const QUrl url(asset.value("browser_download_url").toString(),QUrl::StrictMode);
        const auto size=asset.value("size"); const auto sha=asset.value("digest").toString();
        if (!url.isValid() || url.scheme()!="https" || url.host()!="github.com" || url.port()!=-1 ||
            !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
            url.path()!="/MetaCubeX/mihomo/releases/download/"+tag+'/'+expected || !digest.match(sha).hasMatch() ||
            !size.isDouble() || size.toDouble()<1 || size.toDouble()>ArchiveLimit || size.toDouble()!=double(qint64(size.toDouble())))
            return Result::fail("官方资产地址、大小或SHA-256不完整，未允许下载；可使用本地预检");
        selected={{"version",tag},{"asset",expected},{"url",url.toString()},{"sha256",sha.mid(7)},{"bytes",size}};
    }
    return selected.isEmpty()?Result::fail("官方稳定版未提供Windows amd64-v1压缩包"):Result{true,{},selected};
}
void OfficialCoreSource::check(Completion done,const DownloadPolicy& policy,int port) {
    if (busy_) { done(Result::fail("官方内核任务正在进行")); return; }
    release_={}; busy_=true; const auto token=++generation_;
    done_=[done=std::move(done)](Result result,QByteArray) mutable { done(result); };
    downloader_->download(QUrl("https://api.github.com/repos/MetaCubeX/mihomo/releases/latest"),
        [this,token](Result result,QByteArray bytes) {
            if (token!=generation_) return;
            if (result.ok) result=parseRelease(bytes);
            else result.error="检查官方版本失败（包括GitHub限流）；可稍后重试或本地预检。"+result.error;
            if (result.ok) release_=result.data;
            busy_=false; auto done=std::move(done_); done(result,{});
        },policy,port,2*1024*1024);
}
void OfficialCoreSource::download(DownloadDone done,const DownloadPolicy& policy,int port) {
    if (busy_||release_.isEmpty()) { done(Result::fail("请先检查官方稳定版本"),{}); return; }
    busy_=true; done_=std::move(done); const auto token=++generation_; const auto release=release_;
    auto bounded=policy; bounded.timeoutSeconds=120;
    downloader_->download(QUrl(release.value("url").toString()),[this,token,release](Result result,QByteArray bytes) {
        if (token!=generation_) return;
        if (result.ok&&bytes.size()!=release.value("bytes").toDouble()) result=Result::fail("官方内核下载大小不符，未预检或执行");
        // Hashing and decompression run on the Controller's bounded worker.
        result.data=release; busy_=false; auto done=std::move(done_); done(result,result.ok?bytes:QByteArray{});
    },bounded,port,ArchiveLimit);
}
void OfficialCoreSource::cancel() {
    ++generation_; downloader_->cancel(); busy_=false;
    auto done=std::move(done_); if (done) done(Result{false,"官方内核任务已取消",{{"cancelled",true}}},{});
}
Result OfficialCoreSource::unpack(const QByteArray& archive,const QJsonObject& release,QByteArray& executable) {
    executable.clear();
    if (archive.isEmpty()||archive.size()>ArchiveLimit || archive.size()!=release.value("bytes").toDouble() || hash(archive)!=release.value("sha256").toString())
        return Result::fail("官方内核压缩包SHA-256或大小不符，未解包");
    const auto valid=[&](qint64 at,qint64 size) { return at>=0&&size>=0&&at<=archive.size()&&size<=archive.size()-at; };
    const auto u16=[&](qint64 at) { return qFromLittleEndian<quint16>(archive.constData()+at); };
    const auto u32=[&](qint64 at) { return qFromLittleEndian<quint32>(archive.constData()+at); };
    qint64 end=-1;
    for (qint64 i=archive.size()-22;i>=qMax<qint64>(0,archive.size()-65557);--i)
        if (valid(i,22)&&u32(i)==0x06054b50&&i+22+u16(i+20)==archive.size()) { end=i; break; }
    if (end<0||u16(end+4)||u16(end+6)||u16(end+8)!=u16(end+10)||u16(end+10)<1||u16(end+10)>8)
        return Result::fail("官方压缩包不是受支持的单卷ZIP");
    qint64 cursor=u32(end+16),centralEnd=cursor+u32(end+12),total=0; int found=0;
    if (!valid(cursor,centralEnd-cursor)||centralEnd!=end) return Result::fail("ZIP目录范围无效");
    QByteArray output;
    for (int entry=0;entry<u16(end+10);++entry) {
        if (!valid(cursor,46)||cursor+46>centralEnd||u32(cursor)!=0x02014b50) return Result::fail("ZIP目录损坏");
        const auto flags=u16(cursor+8),method=u16(cursor+10); const auto crc=u32(cursor+16);
        const qint64 compressed=u32(cursor+20),size=u32(cursor+24),local=u32(cursor+42);
        const int names=u16(cursor+28),extra=u16(cursor+30),comment=u16(cursor+32);
        if (!valid(cursor+46,names+extra+comment)||cursor+46+names+extra+comment>centralEnd ||
            (flags&~quint16(0x080e)) || (method!=0&&method!=8) || compressed>ArchiveLimit || size>ExecutableLimit || u16(cursor+34)!=0)
            return Result::fail("ZIP加密、格式或大小不受支持");
        const auto name=archive.mid(cursor+46,names);
        if (name.isEmpty()||name.contains('/')||name.contains('\\')||name.contains("..")||name.contains('\0')) return Result::fail("ZIP文件名无效");
        total+=size; if (total>ExecutableLimit) return Result::fail("ZIP解压总量超过128 MiB");
        if (!valid(local,30)||local+30>u32(end+16)||u32(local)!=0x04034b50 || u16(local+6)!=flags || u16(local+8)!=method)
            return Result::fail("ZIP本地头与目录不一致");
        const int localName=u16(local+26),localExtra=u16(local+28);
        const qint64 data=local+30+localName+localExtra;
        if (!valid(local+30,localName+localExtra)||archive.mid(local+30,localName)!=name || !valid(data,compressed)||data+compressed>u32(end+16))
            return Result::fail("ZIP数据范围或文件名不一致");
        if (name.endsWith(".exe")) {
            if (++found!=1||!name.startsWith("mihomo")||size<64) return Result::fail("ZIP应仅含一个Mihomo程序");
            output.resize(size);
            if (method==0) {
                if (compressed!=size) return Result::fail("ZIP未压缩大小不一致"); output=archive.mid(data,size);
            } else {
                z_stream stream{}; stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(archive.constData()+data)); stream.avail_in=uInt(compressed);
                stream.next_out=reinterpret_cast<Bytef*>(output.data()); stream.avail_out=uInt(size);
                if (inflateInit2(&stream,-MAX_WBITS)!=Z_OK) return Result::fail("ZIP解码器初始化失败");
                const auto status=inflate(&stream,Z_FINISH); const bool ok=status==Z_STREAM_END&&stream.total_out==quint64(size)&&stream.total_in==quint64(compressed);
                inflateEnd(&stream); if (!ok) return Result::fail("ZIP压缩数据损坏或解压超限");
            }
            if (quint32(crc32(0,reinterpret_cast<const Bytef*>(output.constData()),uInt(output.size())))!=crc) return Result::fail("ZIP程序CRC不符");
        }
        cursor+=46+names+extra+comment;
    }
    if (cursor!=centralEnd||found!=1) return Result::fail("ZIP程序缺失或目录不完整");
    executable=std::move(output); return {};
}
}
