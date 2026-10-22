#include "MrsSnapshot.h"
#include <zstd.h>
#include <QtEndian>
#include <memory>

namespace mlash {
namespace {
constexpr qsizetype maxDecoded=64*1024*1024;
struct Reader {
    const QByteArray& data;
    qsizetype offset=0;
    bool take(qsizetype size, QByteArray* bytes=nullptr) {
        if (size<0 || size>data.size()-offset) return false;
        if (bytes) *bytes=data.mid(offset,size);
        offset+=size; return true;
    }
    bool integer(quint64& value) {
        if (data.size()-offset<8) return false;
        value=qFromBigEndian<quint64>(data.constData()+offset); offset+=8; return value<=quint64(maxDecoded);
    }
    bool vector(int width,QByteArray& bytes) {
        quint64 length=0; return integer(length)&&length>0&&length<=quint64((data.size()-offset)/width)&&take(qsizetype(length)*width,&bytes);
    }
};
bool bit(const QByteArray& words,qsizetype index) { return (qFromBigEndian<quint64>(words.constData()+8*(index/64)) >> (index%64)) & 1; }
bool validDomain(Reader& reader) {
    QByteArray leaves,bitmap,labels;
    if (!reader.vector(8,leaves)||!reader.vector(8,bitmap)||!reader.vector(1,labels)) return false;
    const auto nodes=labels.size()+1, bits=2*labels.size()+1;
    if (leaves.size()!=((nodes+63)/64)*8 || bitmap.size()!=((bits+63)/64)*8) return false;
    qsizetype pending=1, ones=0;
    for (qsizetype i=0;i<bits;++i) { if (bit(bitmap,i)) { --pending; ++ones; } else ++pending; if (pending<0 || (!pending&&i!=bits-1)) return false; }
    if (pending || ones!=nodes) return false;
    for (qsizetype i=bits;i<(bitmap.size()/8)*64;++i) if (bit(bitmap,i)) return false;
    for (qsizetype i=nodes;i<(leaves.size()/8)*64;++i) if (bit(leaves,i)) return false;
    return true;
}
}
Result inspectMrs(const QByteArray& compressed,const QString& expectedBehavior) {
    const auto invalid=[] { return Result::fail("MRS快照压缩、版本、行为或结构无效（解压最多64MiB）"); };
    if (compressed.isEmpty()||compressed.size()>8*1024*1024) return invalid();
    std::unique_ptr<ZSTD_DCtx,decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(),ZSTD_freeDCtx);
    if (!context || ZSTD_isError(ZSTD_DCtx_setParameter(context.get(),ZSTD_d_windowLogMax,26))) return invalid();
    ZSTD_inBuffer input{compressed.constData(),size_t(compressed.size()),0}; QByteArray decoded;
    QByteArray chunk(64*1024,Qt::Uninitialized); size_t remaining=1;
    while (remaining) {
        ZSTD_outBuffer output{chunk.data(),size_t(chunk.size()),0}; const auto before=input.pos;
        remaining=ZSTD_decompressStream(context.get(),&output,&input);
        if (ZSTD_isError(remaining)||qsizetype(output.pos)>maxDecoded-decoded.size()) return invalid();
        decoded.append(chunk.constData(),qsizetype(output.pos));
        if (remaining && input.pos==before && !output.pos) return invalid();
    }
    if (input.pos!=input.size || decoded.size()<22 || decoded.left(4)!=QByteArray("MRS\1",4)) return invalid();
    const auto behavior=quint8(decoded[4]); const QString kind=behavior==0?"domain":behavior==1?"ipcidr":"";
    if (kind.isEmpty()||(!expectedBehavior.isEmpty()&&kind!=expectedBehavior)) return invalid();
    Reader reader{decoded,5}; quint64 count=0,extra=0;
    if (!reader.integer(count)||!count||count>10000000||!reader.integer(extra)||!reader.take(qsizetype(extra))||reader.offset>=decoded.size()||decoded[reader.offset++]!=1) return invalid();
    if (behavior==0) { if (!validDomain(reader)) return invalid(); }
    else {
        QByteArray ranges; if (!reader.vector(32,ranges)) return invalid();
        for (qsizetype i=0;i<ranges.size();i+=32) if (ranges.mid(i,16).compare(ranges.mid(i+16,16))>0) return invalid();
    }
    if (reader.offset!=decoded.size()) return invalid();
    return {true,{},{{"count",int(count)},{"behavior",kind},{"decoded_bytes",double(decoded.size())}}};
}
}
