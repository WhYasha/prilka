#include "JwtService.h"
#include "../config/Config.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <json/json.h>
#include <stdexcept>
#include <sstream>
#include <cstring>

JwtService& JwtService::instance() {
    static JwtService inst;
    return inst;
}

// ── Base64URL ──────────────────────────────────────────────────────────────

std::string JwtService::b64url_encode(const unsigned char* data, size_t len) {
    // Standard base64 via OpenSSL BIO
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);

    // Base64 → Base64URL
    for (auto& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Remove padding
    while (!result.empty() && result.back() == '=')
        result.pop_back();
    return result;
}

static std::string b64url_encode_str(const std::string& s) {
    return JwtService::b64url_encode(
        reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

std::string JwtService::b64url_decode_str(const std::string& in) {
    std::string padded = in;
    for (auto& c : padded) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (padded.size() % 4) padded += '=';

    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(padded.data(), static_cast<int>(padded.size()));
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    std::string result(padded.size(), '\0');
    int decoded = BIO_read(b64, result.data(), static_cast<int>(padded.size()));
    BIO_free_all(b64);
    if (decoded < 0) return {};
    result.resize(static_cast<size_t>(decoded));
    return result;
}

// ── HMAC-SHA256 ───────────────────────────────────────────────────────────

std::string JwtService::sign(const std::string& headerDotPayload) const {
    const auto& secret = Config::get().jwtSecret;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  dlen = 0;

    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(headerDotPayload.data()),
         headerDotPayload.size(),
         digest, &dlen);

    return b64url_encode(digest, dlen);
}

// ── Payload builder ────────────────────────────────────────────────────────

std::string JwtService::makePayload(long long userId,
                                    const std::string& type, int ttl) {
    long long now = static_cast<long long>(std::time(nullptr));
    Json::Value p;
    p["sub"]  = std::to_string(userId);
    p["type"] = type;
    p["iat"]  = Json::Int64(now);
    p["exp"]  = Json::Int64(now + ttl);

    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "";
    return Json::writeString(wbuilder, p);
}

// ── Public API ─────────────────────────────────────────────────────────────

static const std::string kHeader =
    b64url_encode_str(R"({"alg":"HS256","typ":"JWT"})");

std::string JwtService::createAccessToken(long long userId) const {
    auto payload = b64url_encode_str(
        makePayload(userId, "access", Config::get().jwtAccessTtl));
    auto hdrPayload = kHeader + "." + payload;
    return hdrPayload + "." + sign(hdrPayload);
}

std::string JwtService::createRefreshToken(long long userId) const {
    auto payload = b64url_encode_str(
        makePayload(userId, "refresh", Config::get().jwtRefreshTtl));
    auto hdrPayload = kHeader + "." + payload;
    return hdrPayload + "." + sign(hdrPayload);
}

std::optional<JwtService::Claims> JwtService::verify(const std::string& token) const {
    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return std::nullopt;
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return std::nullopt;

    std::string hdrPayload = token.substr(0, dot2);
    std::string sigGiven   = token.substr(dot2 + 1);
    std::string sigExpect  = sign(hdrPayload);

    // Constant-time comparison
    if (sigGiven.size() != sigExpect.size()) return std::nullopt;
    unsigned char diff = 0;
    for (size_t i = 0; i < sigGiven.size(); i++)
        diff |= static_cast<unsigned char>(sigGiven[i]) ^
                static_cast<unsigned char>(sigExpect[i]);
    if (diff != 0) return std::nullopt;

    // Decode payload
    std::string payloadJson = b64url_decode_str(token.substr(dot1 + 1, dot2 - dot1 - 1));
    if (payloadJson.empty()) return std::nullopt;

    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    std::istringstream ss(payloadJson);
    std::string err;
    if (!Json::parseFromStream(rbuilder, ss, &root, &err)) return std::nullopt;

    long long exp = root["exp"].asInt64();
    long long now = static_cast<long long>(std::time(nullptr));
    if (now >= exp) return std::nullopt;  // expired

    Claims c;
    c.userId    = std::stoll(root["sub"].asString());
    c.tokenType = root["type"].asString();
    c.exp       = exp;
    c.iat       = root["iat"].asInt64();
    return c;
}
