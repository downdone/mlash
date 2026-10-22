#include "platform/PortableCrypto.h"
#include <windows.h>
#include <bcrypt.h>

namespace mlash {
namespace {
constexpr int MaxPayload = 96 * 1024 * 1024;
const QByteArray Magic("MLASHB01");
struct Algorithm {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~Algorithm() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
struct Key {
    BCRYPT_KEY_HANDLE handle = nullptr;
    QByteArray storage;
    ~Key() { if (handle) BCryptDestroyKey(handle); if (!storage.isEmpty()) SecureZeroMemory(storage.data(), size_t(storage.size())); }
};
struct SecretBytes {
    QByteArray bytes;
    explicit SecretBytes(QByteArray value) : bytes(std::move(value)) {}
    ~SecretBytes() { if (!bytes.isEmpty()) SecureZeroMemory(bytes.data(), size_t(bytes.size())); }
};
PUCHAR data(QByteArray& bytes) { return reinterpret_cast<PUCHAR>(bytes.data()); }
Result transform(bool encrypt, const QByteArray& input, const QString& password, QByteArray& output) {
    output.clear();
    if (password.size() < 8 || password.size() > 1024 || input.isEmpty() || input.size() > MaxPayload || (!encrypt && (input.size() < 53 || !input.startsWith(Magic)))) return Result::fail("备份格式/大小无效，或密码长度不在 8–1024 字符内");
    QByteArray salt(16, 0), nonce(12, 0), tag(16, 0), payload;
    if (encrypt) {
        if (BCryptGenRandom(nullptr, data(salt), ULONG(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0 || BCryptGenRandom(nullptr, data(nonce), ULONG(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return Result::fail("无法生成备份随机数");
        payload = input;
    } else { salt = input.mid(8,16); nonce = input.mid(24,12); tag = input.mid(36,16); payload = input.mid(52); }
    Algorithm hash, aes;
    if (BCryptOpenAlgorithmProvider(&hash.handle, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0 || BCryptOpenAlgorithmProvider(&aes.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) return Result::fail("系统备份加密算法不可用");
    SecretBytes passwordBytes(password.toUtf8()), derived(QByteArray(32, 0));
    if (BCryptDeriveKeyPBKDF2(hash.handle, data(passwordBytes.bytes), ULONG(passwordBytes.bytes.size()), data(salt), ULONG(salt.size()), 600000, data(derived.bytes), ULONG(derived.bytes.size()), 0) < 0) return Result::fail("备份密钥派生失败");
    if (BCryptSetProperty(aes.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0) < 0) return Result::fail("系统不支持备份认证加密");
    DWORD objectSize = 0, received = 0; Key key;
    if (BCryptGetProperty(aes.handle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0) < 0 || objectSize > 1024 * 1024) return Result::fail("备份密钥对象无效");
    key.storage.resize(objectSize);
    if (BCryptGenerateSymmetricKey(aes.handle, &key.handle, data(key.storage), objectSize, data(derived.bytes), ULONG(derived.bytes.size()), 0) < 0) return Result::fail("备份加密密钥初始化失败");
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info; BCRYPT_INIT_AUTH_MODE_INFO(info);
    auto authenticated = Magic + salt + nonce;
    info.pbNonce = data(nonce); info.cbNonce = ULONG(nonce.size()); info.pbTag = data(tag); info.cbTag = ULONG(tag.size()); info.pbAuthData = data(authenticated); info.cbAuthData = ULONG(authenticated.size());
    QByteArray transformed(payload.size(), 0); ULONG size = 0;
    const auto status = encrypt ? BCryptEncrypt(key.handle, data(payload), ULONG(payload.size()), &info, nullptr, 0, data(transformed), ULONG(transformed.size()), &size, 0) : BCryptDecrypt(key.handle, data(payload), ULONG(payload.size()), &info, nullptr, 0, data(transformed), ULONG(transformed.size()), &size, 0);
    if (status < 0 || size != ULONG(payload.size())) { SecureZeroMemory(transformed.data(), size_t(transformed.size())); return Result::fail("备份密码不正确、内容被修改或加密失败"); }
    output = encrypt ? Magic + salt + nonce + tag + transformed : transformed;
    return {};
}
}
Result sealPortable(const QByteArray& plain, const QString& password, QByteArray& encrypted) { return transform(true, plain, password, encrypted); }
Result openPortable(const QByteArray& encrypted, const QString& password, QByteArray& plain) { return transform(false, encrypted, password, plain); }
}
