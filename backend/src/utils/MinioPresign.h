#pragma once
// MinioPresign.h — AWS SigV4 presigned GET URL generator for MinIO
// Header-only: include wherever presigned URLs are needed.

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace minio_presign {

static inline std::string toHex(const unsigned char* d, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}

static inline std::string sha256Hex(const std::string& s) {
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), h);
    return toHex(h, SHA256_DIGEST_LENGTH);
}

static inline std::vector<unsigned char> hmacSha256Raw(const std::string& key,
                                                        const std::string& msg) {
    std::vector<unsigned char> h(32); unsigned int hlen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         h.data(), &hlen);
    h.resize(hlen);
    return h;
}

static inline std::vector<unsigned char> hmacSha256Raw(const std::vector<unsigned char>& key,
                                                        const std::string& msg) {
    std::vector<unsigned char> h(32); unsigned int hlen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         h.data(), &hlen);
    h.resize(hlen);
    return h;
}

static inline std::vector<unsigned char> sigV4SigningKey(const std::string& secretKey,
                                                          const std::string& date,
                                                          const std::string& region,
                                                          const std::string& service) {
    auto kDate    = hmacSha256Raw("AWS4" + secretKey, date);
    auto kRegion  = hmacSha256Raw(kDate, region);
    auto kService = hmacSha256Raw(kRegion, service);
    return hmacSha256Raw(kService, "aws4_request");
}

// Generate a presigned GET URL for MinIO (S3-compatible).
// endpoint:  "minio:9000" or "http://minio:9000"
// publicUrl: if non-empty, rewrites the internal base URL to this
//            (e.g., "https://behappy.rest/minio")
// ttlSeconds: URL expiry in seconds
inline std::string generatePresignedUrl(const std::string& endpoint,
                                         const std::string& publicUrl,
                                         const std::string& bucket,
                                         const std::string& key,
                                         const std::string& accessKey,
                                         const std::string& secretKey,
                                         int ttlSeconds = 900) {
    if (key.empty()) return "";

    const std::string region  = "us-east-1";
    const std::string service = "s3";

    std::time_t now = std::time(nullptr);
    char dateBuf[9], timeBuf[17];
    std::tm* utc = std::gmtime(&now);
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", utc);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%dT%H%M%SZ", utc);
    std::string date(dateBuf), datetime(timeBuf);

    std::string uri = "/" + bucket + "/" + key;

    std::string credScope = date + "/" + region + "/" + service + "/aws4_request";
    std::string credential = accessKey + "/" + credScope;

    std::ostringstream qs;
    qs << "X-Amz-Algorithm=AWS4-HMAC-SHA256"
       << "&X-Amz-Credential=" << credential
       << "&X-Amz-Date=" << datetime
       << "&X-Amz-Expires=" << ttlSeconds
       << "&X-Amz-SignedHeaders=host";

    std::string host = endpoint;
    if (host.find("://") != std::string::npos)
        host = host.substr(host.find("://") + 3);

    std::string canonicalRequest =
        "GET\n" + uri + "\n" + qs.str() + "\nhost:" + host + "\n\nhost\nUNSIGNED-PAYLOAD";

    std::string stringToSign =
        "AWS4-HMAC-SHA256\n" + datetime + "\n" + credScope + "\n" +
        sha256Hex(canonicalRequest);

    auto kSigning = sigV4SigningKey(secretKey, date, region, service);
    unsigned char h[32]; unsigned int hlen = 0;
    HMAC(EVP_sha256(), kSigning.data(), static_cast<int>(kSigning.size()),
         reinterpret_cast<const unsigned char*>(stringToSign.data()),
         stringToSign.size(), h, &hlen);
    std::string signature = toHex(h, hlen);

    std::string scheme = "http://";
    std::string url = scheme + host + uri + "?" + qs.str() + "&X-Amz-Signature=" + signature;

    if (!publicUrl.empty()) {
        std::string internalBase = scheme + host;
        if (url.rfind(internalBase, 0) == 0)
            url = publicUrl + url.substr(internalBase.size());
    }
    return url;
}

} // namespace minio_presign
