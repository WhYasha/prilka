#include "FilesController.h"
#include "../config/Config.h"
#include <drogon/orm/DbClient.h>
#include <drogon/HttpClient.h>
#include <drogon/MultiPart.h>

// contentTypeToMime is defined in libdrogon but not exposed in a public header.
namespace drogon {
    const std::string_view& contentTypeToMime(ContentType ct);
}
#include <trantor/utils/Logger.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <json/json.h>
#include <uuid/uuid.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

// ── Helpers ────────────────────────────────────────────────────────────────

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

static std::string toHex(const unsigned char* d, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}

static std::string sha256Hex(const std::string& s) {
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), h);
    return toHex(h, SHA256_DIGEST_LENGTH);
}

static std::string hmacSha256Hex(const std::string& key, const std::string& msg) {
    unsigned char h[32]; unsigned int hlen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         h, &hlen);
    return toHex(h, hlen);
}

static std::vector<unsigned char> hmacSha256Raw(const std::string& key, const std::string& msg) {
    std::vector<unsigned char> h(32); unsigned int hlen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         h.data(), &hlen);
    h.resize(hlen);
    return h;
}

static std::vector<unsigned char> hmacSha256Raw(const std::vector<unsigned char>& key,
                                                 const std::string& msg) {
    std::vector<unsigned char> h(32); unsigned int hlen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         h.data(), &hlen);
    h.resize(hlen);
    return h;
}

// ── AWS Signature V4 helpers ────────────────────────────────────────────────

// Build the four-stage signing key and return as raw bytes.
static std::vector<unsigned char> sigV4SigningKey(const std::string& secretKey,
                                                   const std::string& date,
                                                   const std::string& region,
                                                   const std::string& service) {
    auto kDate    = hmacSha256Raw("AWS4" + secretKey, date);
    auto kRegion  = hmacSha256Raw(kDate, region);
    auto kService = hmacSha256Raw(kRegion, service);
    return hmacSha256Raw(kService, "aws4_request");
}

// Produce a SigV4 Authorization header for a direct PUT to MinIO.
// Signs: host, x-amz-content-sha256, x-amz-date.
static void sigV4PutHeaders(const std::string& host,
                             const std::string& uri,
                             const std::string& accessKey,
                             const std::string& secretKey,
                             const std::string& contentType,
                             const std::string& body,
                             // out-params
                             std::string& outDate,
                             std::string& outContentSha,
                             std::string& outAuth) {
    const std::string region  = "us-east-1";
    const std::string service = "s3";

    std::time_t now = std::time(nullptr);
    char dateBuf[9], timeBuf[17];
    std::tm* utc = std::gmtime(&now);
    std::strftime(dateBuf, sizeof(dateBuf),  "%Y%m%d",        utc);
    std::strftime(timeBuf, sizeof(timeBuf),  "%Y%m%dT%H%M%SZ", utc);
    std::string date(dateBuf), datetime(timeBuf);

    outDate        = datetime;
    outContentSha  = sha256Hex(body);

    // Canonical headers (must be sorted alphabetically)
    std::string canonicalHeaders =
        "content-type:" + contentType + "\n"
        "host:" + host + "\n"
        "x-amz-content-sha256:" + outContentSha + "\n"
        "x-amz-date:" + datetime + "\n";
    std::string signedHeaders = "content-type;host;x-amz-content-sha256;x-amz-date";

    std::string canonicalRequest =
        "PUT\n" + uri + "\n\n" +
        canonicalHeaders + "\n" +
        signedHeaders + "\n" +
        outContentSha;

    std::string credScope = date + "/" + region + "/" + service + "/aws4_request";
    std::string stringToSign =
        "AWS4-HMAC-SHA256\n" + datetime + "\n" + credScope + "\n" +
        sha256Hex(canonicalRequest);

    auto kSigning = sigV4SigningKey(secretKey, date, region, service);
    unsigned char h[32]; unsigned int hlen = 0;
    HMAC(EVP_sha256(), kSigning.data(), static_cast<int>(kSigning.size()),
         reinterpret_cast<const unsigned char*>(stringToSign.data()),
         stringToSign.size(), h, &hlen);
    std::string signature = toHex(h, hlen);

    outAuth = "AWS4-HMAC-SHA256 Credential=" + accessKey + "/" + credScope +
              ", SignedHeaders=" + signedHeaders +
              ", Signature=" + signature;
}

// ── AWS Signature V4 Presigned URL ─────────────────────────────────────────
// Generates a presigned GET URL for MinIO (S3-compatible).

// publicUrl: if non-empty, the returned URL's "http://<endpoint>" prefix is
// replaced with this value (e.g. "https://behappy.rest/minio").
// nginx must forward the /minio/ location with Host: minio:9000 so MinIO
// validates the signature against the internal host used during signing.
static std::string generatePresignedUrl(const std::string& endpoint,
                                         const std::string& publicUrl,
                                         const std::string& bucket,
                                         const std::string& key,
                                         const std::string& accessKey,
                                         const std::string& secretKey,
                                         int ttlSeconds = 900) {
    const std::string region  = "us-east-1";
    const std::string service = "s3";

    // Date/time
    std::time_t now = std::time(nullptr);
    char dateBuf[9], timeBuf[17];
    std::tm* utc = std::gmtime(&now);
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", utc);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%dT%H%M%SZ", utc);

    std::string date(dateBuf);
    std::string datetime(timeBuf);

    // Canonical URI
    std::string uri = "/" + bucket + "/" + key;

    // Credential scope
    std::string credScope = date + "/" + region + "/" + service + "/aws4_request";
    std::string credential = accessKey + "/" + credScope;

    // Query string (sorted)
    std::ostringstream qs;
    qs << "X-Amz-Algorithm=AWS4-HMAC-SHA256"
       << "&X-Amz-Credential=" << credential
       << "&X-Amz-Date=" << datetime
       << "&X-Amz-Expires=" << ttlSeconds
       << "&X-Amz-SignedHeaders=host";

    // Parse host from endpoint
    std::string host = endpoint;
    if (host.find("://") != std::string::npos)
        host = host.substr(host.find("://") + 3);

    std::string canonicalRequest =
        "GET\n" + uri + "\n" + qs.str() + "\nhost:" + host + "\n\nhost\nUNSIGNED-PAYLOAD";

    std::string stringToSign =
        "AWS4-HMAC-SHA256\n" + datetime + "\n" + credScope + "\n" +
        sha256Hex(canonicalRequest);

    // Signing key
    auto kSigning = sigV4SigningKey(secretKey, date, region, service);
    unsigned char h[32]; unsigned int hlen = 0;
    HMAC(EVP_sha256(), kSigning.data(), static_cast<int>(kSigning.size()),
         reinterpret_cast<const unsigned char*>(stringToSign.data()),
         stringToSign.size(), h, &hlen);
    std::string signature = toHex(h, hlen);

    std::string scheme = "http://";
    std::string url = scheme + host + uri + "?" + qs.str() + "&X-Amz-Signature=" + signature;

    // Rewrite internal endpoint to public URL if configured.
    if (!publicUrl.empty()) {
        std::string internalBase = scheme + host;
        if (url.rfind(internalBase, 0) == 0)
            url = publicUrl + url.substr(internalBase.size());
    }
    return url;
}

// ── UUID generator ─────────────────────────────────────────────────────────

static std::string newUuid() {
    uuid_t u;
    uuid_generate_random(u);
    char s[37]; uuid_unparse_lower(u, s);
    return std::string(s);
}

// ── POST /files ────────────────────────────────────────────────────────────
// Multipart upload. Stores object in MinIO, metadata in PostgreSQL.

void FilesController::uploadFile(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    const auto& cfg = Config::get();

    // Validate size
    long long maxBytes = cfg.maxFileSizeMb * 1024 * 1024;
    if (static_cast<long long>(req->body().size()) > maxBytes)
        return cb(jsonErr("File too large (max " + std::to_string(cfg.maxFileSizeMb) + " MB)",
                          drogon::k413RequestEntityTooLarge));

    drogon::MultiPartParser mp;
    if (mp.parse(req) != 0 || mp.getFiles().empty())
        return cb(jsonErr("No file uploaded (use multipart/form-data field 'file')",
                          drogon::k400BadRequest));

    const auto& files = mp.getFiles();
    const auto& f     = files[0];
    std::string orig  = f.getFileName();
    std::string mime  = std::string(drogon::contentTypeToMime(f.getContentType()));
    std::string objectKey= "uploads/" + newUuid() + "_" + orig;
    std::string bucket   = cfg.minioUploadsBucket;

    // Upload to MinIO via HTTP PUT using AWS Signature V4.
    std::string minioHost = cfg.minioEndpoint;
    auto client = drogon::HttpClient::newHttpClient("http://" + minioHost);

    std::string uri      = "/" + bucket + "/" + objectKey;
    std::string bodyStr  = std::string(f.fileData(), f.fileLength());

    std::string amzDate, contentSha, authHeader;
    sigV4PutHeaders(minioHost, uri,
                    cfg.minioAccessKey, cfg.minioSecretKey,
                    mime, bodyStr,
                    amzDate, contentSha, authHeader);

    auto putReq = drogon::HttpRequest::newHttpRequest();
    putReq->setMethod(drogon::Put);
    putReq->setPath(uri);
    putReq->setContentTypeString(mime);
    putReq->setBody(bodyStr);
    putReq->addHeader("x-amz-date",             amzDate);
    putReq->addHeader("x-amz-content-sha256",   contentSha);
    putReq->addHeader("Authorization",           authHeader);

    client->sendRequest(putReq, [=, cb = std::move(cb)](
            drogon::ReqResult result,
            const drogon::HttpResponsePtr& minioResp) mutable {

        if (result != drogon::ReqResult::Ok || !minioResp ||
            minioResp->statusCode() >= 300) {
            LOG_ERROR << "MinIO PUT failed: "
                      << (minioResp ? std::to_string(minioResp->statusCode()) : "no response");
            return cb(jsonErr("Object storage upload failed", drogon::k502BadGateway));
        }

        // Persist metadata
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes) "
            "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
            [cb, objectKey, bucket, orig, mime](const drogon::orm::Result& r) mutable {
                long long fileId = r[0]["id"].as<long long>();
                Json::Value resp;
                resp["id"]         = Json::Int64(fileId);
                resp["filename"]   = orig;
                resp["mime_type"]  = mime;
                resp["object_key"] = objectKey;
                auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
                httpResp->setStatusCode(drogon::k201Created);
                cb(httpResp);
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "file metadata insert: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            me, bucket, objectKey, orig, mime, static_cast<long long>(0));
    });
}

// ── GET /files/{id}/download ───────────────────────────────────────────────
// Returns a presigned URL redirect to MinIO.

void FilesController::downloadFile(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                    long long fileId) {
    const auto& cfg = Config::get();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT bucket, object_key, filename, mime_type FROM files WHERE id = $1",
        [cb, &cfg](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("File not found", drogon::k404NotFound));

            std::string bucket    = r[0]["bucket"].as<std::string>();
            std::string objectKey = r[0]["object_key"].as<std::string>();

            std::string presignedUrl = generatePresignedUrl(
                cfg.minioEndpoint,
                cfg.minioPublicUrl,
                bucket, objectKey,
                cfg.minioAccessKey, cfg.minioSecretKey,
                cfg.presignTtl);

            auto resp = drogon::HttpResponse::newRedirectionResponse(presignedUrl);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "downloadFile: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, fileId);
}
