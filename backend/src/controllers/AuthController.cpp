#include "AuthController.h"
#include "../services/JwtService.h"
#include "../config/Config.h"
#include "../utils/MinioPresign.h"
#include <drogon/orm/DbClient.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <trantor/utils/Logger.h>
#include <sstream>
#include <iomanip>

// ── Password hashing (PBKDF2-SHA256) ──────────────────────────────────────

static std::string toHex(const unsigned char* d, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)d[i];
    return ss.str();
}

static std::string randomHex(int bytes) {
    std::vector<unsigned char> buf(bytes);
    RAND_bytes(buf.data(), bytes);
    return toHex(buf.data(), bytes);
}

static std::string hashPassword(const std::string& password, const std::string& salt) {
    unsigned char out[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                      reinterpret_cast<const unsigned char*>(salt.c_str()),
                      static_cast<int>(salt.size()),
                      100000, EVP_sha256(), 32, out);
    return "pbkdf2$" + salt + "$" + toHex(out, 32);
}

static bool verifyPassword(const std::string& password, const std::string& stored) {
    // Format: "pbkdf2$<salt>$<hash>"
    auto p1 = stored.find('$');
    auto p2 = stored.find('$', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;
    std::string salt = stored.substr(p1 + 1, p2 - p1 - 1);
    return hashPassword(password, salt) == stored;
}

// ── Helpers ────────────────────────────────────────────────────────────────

static drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}

static drogon::HttpResponsePtr err(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    return jsonResp(b, code);
}

// ── POST /auth/register ────────────────────────────────────────────────────

void AuthController::registerUser(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) return cb(err("Invalid JSON", drogon::k400BadRequest));

    std::string username = (*body)["username"].asString();
    std::string email    = (*body)["email"].asString();
    std::string password = (*body)["password"].asString();

    if (username.size() < 3 || username.size() > 32)
        return cb(err("Username must be 3–32 characters", drogon::k400BadRequest));
    if (email.empty() || email.find('@') == std::string::npos)
        return cb(err("Invalid email", drogon::k400BadRequest));
    if (password.size() < 6)
        return cb(err("Password must be at least 6 characters", drogon::k400BadRequest));

    std::string salt = randomHex(16);
    std::string hash = hashPassword(password, salt);
    std::string displayName = (*body).get("display_name", username).asString();

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO users (username, email, password_hash, display_name) "
        "VALUES ($1, $2, $3, $4) RETURNING id",
        [cbSh, username](const drogon::orm::Result& r) mutable {
            long long uid = r[0]["id"].as<long long>();
            // Create default settings
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "INSERT INTO user_settings (user_id) VALUES ($1) ON CONFLICT DO NOTHING",
                [](const drogon::orm::Result&) {},
                [](const drogon::orm::DrogonDbException& e) {
                    LOG_WARN << "settings insert error: " << e.base().what();
                }, uid);

            Json::Value resp;
            resp["id"]       = Json::Int64(uid);
            resp["username"] = username;
            (*cbSh)(jsonResp(resp, drogon::k201Created));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) mutable {
            std::string what = e.base().what();
            if (what.find("unique") != std::string::npos ||
                what.find("duplicate") != std::string::npos)
                (*cbSh)(err("Username or email already taken", drogon::k409Conflict));
            else {
                LOG_ERROR << "register DB error: " << what;
                (*cbSh)(err("Internal error", drogon::k500InternalServerError));
            }
        },
        username, email, hash, displayName);
}

// ── POST /auth/login ───────────────────────────────────────────────────────

void AuthController::loginUser(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) return cb(err("Invalid JSON", drogon::k400BadRequest));

    std::string username = (*body)["username"].asString();
    std::string password = (*body)["password"].asString();

    if (username.empty() || password.empty())
        return cb(err("username and password required", drogon::k400BadRequest));

    using CbPtr = std::shared_ptr<std::function<void(const drogon::HttpResponsePtr&)>>;
    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, password_hash, is_blocked FROM users WHERE username = $1 AND is_active = TRUE",
        [cbSh, password](const drogon::orm::Result& r) mutable {
            auto& cb = *cbSh;
            if (r.empty()) return cb(err("Invalid credentials", drogon::k401Unauthorized));

            long long uid  = r[0]["id"].as<long long>();
            std::string stored = r[0]["password_hash"].as<std::string>();
            bool isBlocked = r[0]["is_blocked"].as<bool>();

            if (isBlocked)
                return cb(err("Account blocked. Contact support.", drogon::k403Forbidden));

            if (!verifyPassword(password, stored))
                return cb(err("Invalid credentials", drogon::k401Unauthorized));

            auto& jwt = JwtService::instance();
            std::string accessToken  = jwt.createAccessToken(uid);
            std::string refreshToken = jwt.createRefreshToken(uid);

            // Persist refresh token hash
            std::string tokenHash;
            {
                unsigned char digest[32];
                unsigned int dlen = 0;
                HMAC(EVP_sha256(), "refresh-hash-key", 16,
                     reinterpret_cast<const unsigned char*>(refreshToken.data()),
                     refreshToken.size(), digest, &dlen);
                std::ostringstream ss;
                for (unsigned int i = 0; i < dlen; i++)
                    ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
                tokenHash = ss.str();
            }

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "INSERT INTO refresh_tokens (user_id, token_hash, expires_at) "
                "VALUES ($1, $2, NOW() + INTERVAL '7 days')",
                [](const drogon::orm::Result&) {},
                [](const drogon::orm::DrogonDbException& e) {
                    LOG_WARN << "refresh token persist error: " << e.base().what();
                }, uid, tokenHash);

            // Update last_activity for admin dashboard metrics
            db2->execSqlAsync(
                "UPDATE users SET last_activity = NOW() WHERE id = $1",
                [](const drogon::orm::Result&) {},
                [](const drogon::orm::DrogonDbException&) {},
                uid);

            Json::Value resp;
            resp["access_token"]  = accessToken;
            resp["refresh_token"] = refreshToken;
            resp["token_type"]    = "Bearer";
            resp["expires_in"]    = Config::get().jwtAccessTtl;
            resp["user_id"]       = Json::Int64(uid);
            cb(jsonResp(resp, drogon::k200OK));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "login DB error: " << e.base().what();
            (*cbSh)(err("Internal error", drogon::k500InternalServerError));
        }, username);
}

// ── POST /auth/refresh ─────────────────────────────────────────────────────

void AuthController::refreshToken(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) return cb(err("Invalid JSON", drogon::k400BadRequest));

    std::string token = (*body)["refresh_token"].asString();
    if (token.empty()) return cb(err("refresh_token required", drogon::k400BadRequest));

    auto claims = JwtService::instance().verify(token);
    if (!claims || claims->tokenType != "refresh")
        return cb(err("Invalid or expired refresh token", drogon::k401Unauthorized));

    long long uid = claims->userId;
    std::string newAccess = JwtService::instance().createAccessToken(uid);

    Json::Value resp;
    resp["access_token"] = newAccess;
    resp["token_type"]   = "Bearer";
    resp["expires_in"]   = Config::get().jwtAccessTtl;
    cb(jsonResp(resp, drogon::k200OK));
}

// ── GET /me ────────────────────────────────────────────────────────────────

void AuthController::getMe(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto uid = req->getAttributes()->get<long long>("user_id");

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT u.id, u.username, u.email, u.display_name, u.bio, u.created_at, u.is_admin, "
        "       f.bucket AS avatar_bucket, f.object_key AS avatar_key "
        "FROM users u LEFT JOIN files f ON f.id = u.avatar_file_id "
        "WHERE u.id = $1",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(err("User not found", drogon::k404NotFound));
            Json::Value resp;
            resp["id"]           = Json::Int64(r[0]["id"].as<long long>());
            resp["username"]     = r[0]["username"].as<std::string>();
            resp["email"]        = r[0]["email"].as<std::string>();
            resp["display_name"] = r[0]["display_name"].isNull()
                                       ? Json::Value(r[0]["username"].as<std::string>())
                                       : Json::Value(r[0]["display_name"].as<std::string>());
            resp["bio"]          = r[0]["bio"].isNull() ? Json::Value() : Json::Value(r[0]["bio"].as<std::string>());
            resp["created_at"]   = r[0]["created_at"].as<std::string>();
            resp["is_admin"]     = r[0]["is_admin"].isNull() ? false : r[0]["is_admin"].as<bool>();

            std::string avBucket = r[0]["avatar_bucket"].isNull() ? "" : r[0]["avatar_bucket"].as<std::string>();
            std::string avKey    = r[0]["avatar_key"].isNull()    ? "" : r[0]["avatar_key"].as<std::string>();
            if (!avKey.empty()) {
                const auto& cfg = Config::get();
                std::string url = minio_presign::generatePresignedUrl(
                    cfg.minioEndpoint, cfg.minioPublicUrl,
                    avBucket, avKey,
                    cfg.minioAccessKey, cfg.minioSecretKey, cfg.presignTtl);
                resp["avatar_url"] = url;
            } else {
                resp["avatar_url"] = Json::Value();
            }
            cb(jsonResp(resp, drogon::k200OK));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getMe DB error: " << e.base().what();
            cb(err("Internal error", drogon::k500InternalServerError));
        }, uid);
}
