#include "UsersController.h"
#include "../config/Config.h"
#include "../utils/MinioPresign.h"
#include "../ws/WsHandler.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <regex>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

static drogon::HttpResponsePtr notFound() {
    return jsonErr("User not found", drogon::k404NotFound);
}

// Build avatar_url from bucket + object_key using config
static std::string avatarUrl(const std::string& bucket, const std::string& key) {
    if (key.empty()) return "";
    const auto& cfg = Config::get();
    return minio_presign::generatePresignedUrl(
        cfg.minioEndpoint, cfg.minioPublicUrl,
        bucket, key,
        cfg.minioAccessKey, cfg.minioSecretKey,
        cfg.presignTtl);
}

static Json::Value buildUserJson(const drogon::orm::Row& r) {
    Json::Value u;
    u["id"]           = Json::Int64(r["id"].as<long long>());
    u["username"]     = r["username"].as<std::string>();
    u["display_name"] = r["display_name"].isNull()
                            ? Json::Value(r["username"].as<std::string>())
                            : Json::Value(r["display_name"].as<std::string>());
    u["bio"]          = r["bio"].isNull() ? Json::Value() : Json::Value(r["bio"].as<std::string>());
    u["is_admin"]     = r["is_admin"].isNull() ? false : r["is_admin"].as<bool>();

    std::string avBucket = r["avatar_bucket"].isNull() ? "" : r["avatar_bucket"].as<std::string>();
    std::string avKey    = r["avatar_key"].isNull()    ? "" : r["avatar_key"].as<std::string>();
    std::string url = avatarUrl(avBucket, avKey);
    u["avatar_url"] = url.empty() ? Json::Value() : Json::Value(url);
    return u;
}

// Compute approximate last-seen bucket from last_activity timestamp
static std::string computeLastSeenBucket(const std::string& lastActivity) {
    // lastActivity is ISO timestamp from DB; we use SQL CASE in query instead
    // This is a fallback — actual bucket is computed in SQL
    return lastActivity;
}

// GET /users/{id}
void UsersController::getUser(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                               long long userId) {
    long long viewerId = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT u.id, u.username, u.display_name, u.bio, u.is_admin, "
        "       u.last_activity, "
        "       f.bucket AS avatar_bucket, f.object_key AS avatar_key, "
        "       COALESCE(us.last_seen_visibility, 'everyone') AS last_seen_visibility, "
        "       CASE "
        "         WHEN u.last_activity IS NULL THEN NULL "
        "         WHEN u.last_activity > NOW() - INTERVAL '5 minutes' THEN 'just now' "
        "         WHEN u.last_activity > NOW() - INTERVAL '1 hour' THEN 'within an hour' "
        "         WHEN u.last_activity > NOW() - INTERVAL '1 day' THEN 'today' "
        "         WHEN u.last_activity > NOW() - INTERVAL '7 days' THEN 'this week' "
        "         ELSE 'long ago' "
        "       END AS last_seen_bucket "
        "FROM users u "
        "LEFT JOIN files f ON f.id = u.avatar_file_id "
        "LEFT JOIN user_settings us ON us.user_id = u.id "
        "WHERE u.id = $1 AND u.is_active = TRUE",
        [cb, viewerId](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(notFound());
            Json::Value u = buildUserJson(r[0]);

            bool viewerIsAdmin = false;
            // Check if viewer is admin
            auto db2 = drogon::app().getDbClient();
            // We already have the target user's data; check viewer admin inline
            long long targetId = r[0]["id"].as<long long>();
            bool targetIsAdmin = r[0]["is_admin"].isNull() ? false : r[0]["is_admin"].as<bool>();
            std::string visibility = r[0]["last_seen_visibility"].as<std::string>();
            bool isOnline = WsHandler::isUserOnline(targetId);

            std::string lastActivity;
            if (!r[0]["last_activity"].isNull())
                lastActivity = r[0]["last_activity"].as<std::string>();

            std::string lastSeenBucket;
            if (!r[0]["last_seen_bucket"].isNull())
                lastSeenBucket = r[0]["last_seen_bucket"].as<std::string>();

            // Check if viewer is admin to decide presence rules
            db2->execSqlAsync(
                "SELECT is_admin FROM users WHERE id = $1",
                [cb, u, visibility, isOnline, lastActivity, lastSeenBucket, viewerId, targetId]
                (const drogon::orm::Result& vr) mutable {
                    bool viewerIsAdmin = (!vr.empty() && !vr[0]["is_admin"].isNull() && vr[0]["is_admin"].as<bool>());

                    if (viewerIsAdmin || viewerId == targetId) {
                        // Admin or self: always return exact presence
                        u["is_online"] = isOnline;
                        u["last_activity"] = lastActivity.empty() ? Json::Value() : Json::Value(lastActivity);
                    } else if (visibility == "everyone") {
                        u["is_online"] = isOnline;
                        u["last_activity"] = lastActivity.empty() ? Json::Value() : Json::Value(lastActivity);
                    } else if (visibility == "approx_only") {
                        u["is_online"] = Json::Value();
                        u["last_activity"] = Json::Value();
                        u["last_seen_approx"] = lastSeenBucket.empty() ? Json::Value() : Json::Value(lastSeenBucket);
                    } else {
                        // "nobody"
                        u["is_online"] = Json::Value();
                        u["last_activity"] = Json::Value();
                    }

                    cb(drogon::HttpResponse::newHttpJsonResponse(u));
                },
                [cb, u](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "getUser viewer check: " << e.base().what();
                    // Fallback: return user without presence
                    cb(drogon::HttpResponse::newHttpJsonResponse(u));
                }, viewerId);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getUser: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, userId);
}

// GET /users/by-username/{username}
void UsersController::getUserByUsername(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                        const std::string& username) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT u.id, u.username, u.display_name, u.bio, u.is_admin, "
        "       f.bucket AS avatar_bucket, f.object_key AS avatar_key "
        "FROM users u LEFT JOIN files f ON f.id = u.avatar_file_id "
        "WHERE u.username = $1 AND u.is_active = TRUE",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(notFound());
            cb(drogon::HttpResponse::newHttpJsonResponse(buildUserJson(r[0])));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getUserByUsername: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, username);
}

// GET /users/search?q=alice
void UsersController::searchUsers(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    std::string q = req->getParameter("q");
    // Allow empty q — returns all users (useful for DM user picker)
    std::string pattern = "%" + q + "%";
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT u.id, u.username, u.display_name, u.bio, u.is_admin, "
        "       f.bucket AS avatar_bucket, f.object_key AS avatar_key "
        "FROM users u LEFT JOIN files f ON f.id = u.avatar_file_id "
        "WHERE (u.username ILIKE $1 OR u.display_name ILIKE $1) AND u.is_active = TRUE "
        "ORDER BY u.username LIMIT 50",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r)
                arr.append(buildUserJson(row));
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "searchUsers: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, pattern);
}

// GET /users/{id}/avatar  → 302 to presigned MinIO URL
void UsersController::getUserAvatar(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                    long long userId) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT f.bucket, f.object_key "
        "FROM users u JOIN files f ON f.id = u.avatar_file_id "
        "WHERE u.id = $1",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("No avatar", drogon::k404NotFound));
            std::string url = avatarUrl(
                r[0]["bucket"].as<std::string>(),
                r[0]["object_key"].as<std::string>());
            cb(drogon::HttpResponse::newRedirectionResponse(url));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getUserAvatar: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, userId);
}

// PUT /users/{id}  — update display_name, bio, username (own profile only)
void UsersController::updateUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                  long long userId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    if (me != userId)
        return cb(jsonErr("Forbidden", drogon::k403Forbidden));

    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string displayName = (*body).get("display_name", "").asString();
    std::string bio         = (*body).get("bio", "").asString();

    // Optional username change
    bool hasUsername = (*body).isMember("username") && !(*body)["username"].isNull();
    std::string newUsername = hasUsername ? (*body)["username"].asString() : "";

    if (hasUsername) {
        static const std::regex usernameRe("^[a-zA-Z0-9_]{3,20}$");
        if (!std::regex_match(newUsername, usernameRe))
            return cb(jsonErr("Username must be 3-20 characters: letters, digits, underscores only",
                              drogon::k400BadRequest));
    }

    auto db = drogon::app().getDbClient();

    // Build SQL dynamically based on whether username is being changed
    if (hasUsername) {
        db->execSqlAsync(
            "UPDATE users SET display_name = $1, bio = $2, username = $3, updated_at = NOW() "
            "WHERE id = $4 RETURNING id, username, display_name, bio",
            [cb](const drogon::orm::Result& r) mutable {
                if (r.empty()) return cb(jsonErr("User not found", drogon::k404NotFound));
                Json::Value resp;
                resp["id"]           = Json::Int64(r[0]["id"].as<long long>());
                resp["username"]     = r[0]["username"].as<std::string>();
                resp["display_name"] = r[0]["display_name"].isNull() ? Json::Value() : Json::Value(r[0]["display_name"].as<std::string>());
                resp["bio"]          = r[0]["bio"].isNull() ? Json::Value() : Json::Value(r[0]["bio"].as<std::string>());
                cb(drogon::HttpResponse::newHttpJsonResponse(resp));
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                std::string what = e.base().what();
                if (what.find("unique") != std::string::npos ||
                    what.find("duplicate") != std::string::npos)
                    return cb(jsonErr("Username already taken", drogon::k409Conflict));
                LOG_ERROR << "updateUser: " << what;
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, displayName, bio, newUsername, userId);
    } else {
        db->execSqlAsync(
            "UPDATE users SET display_name = $1, bio = $2, updated_at = NOW() WHERE id = $3 "
            "RETURNING id, username, display_name, bio",
            [cb](const drogon::orm::Result& r) mutable {
                if (r.empty()) return cb(jsonErr("User not found", drogon::k404NotFound));
                Json::Value resp;
                resp["id"]           = Json::Int64(r[0]["id"].as<long long>());
                resp["username"]     = r[0]["username"].as<std::string>();
                resp["display_name"] = r[0]["display_name"].isNull() ? Json::Value() : Json::Value(r[0]["display_name"].as<std::string>());
                resp["bio"]          = r[0]["bio"].isNull() ? Json::Value() : Json::Value(r[0]["bio"].as<std::string>());
                cb(drogon::HttpResponse::newHttpJsonResponse(resp));
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "updateUser: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, displayName, bio, userId);
    }
}

// PUT /users/me/avatar  — body: { "file_id": <int> }
void UsersController::updateMyAvatar(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body || !(*body)["file_id"].isIntegral())
        return cb(jsonErr("file_id required", drogon::k400BadRequest));

    long long fileId = (*body)["file_id"].asInt64();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET avatar_file_id = $1, updated_at = NOW() WHERE id = $2",
        [cb, me, fileId](const drogon::orm::Result&) mutable {
            // Fetch updated avatar_url for response
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT f.bucket, f.object_key FROM files f WHERE f.id = $1",
                [cb](const drogon::orm::Result& fr) mutable {
                    Json::Value resp;
                    if (!fr.empty()) {
                        std::string url = avatarUrl(
                            fr[0]["bucket"].as<std::string>(),
                            fr[0]["object_key"].as<std::string>());
                        resp["avatar_url"] = url;
                    }
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_WARN << "updateMyAvatar lookup: " << e.base().what();
                    Json::Value resp; resp["avatar_url"] = Json::Value();
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                }, fileId);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "updateMyAvatar: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, fileId, me);
}
