#include "StickersController.h"
#include "../config/Config.h"
#include "../utils/MinioPresign.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

static std::string presignedUrl(const std::string& bucket, const std::string& key) {
    if (bucket.empty() || key.empty()) return "";
    const auto& cfg = Config::get();
    return minio_presign::generatePresignedUrl(
        cfg.minioEndpoint, cfg.minioPublicUrl,
        bucket, key,
        cfg.minioAccessKey, cfg.minioSecretKey, cfg.presignTtl);
}

// GET /stickers — flat array of all stickers with presigned image URLs
void StickersController::listStickers(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT s.id, s.label, f.bucket, f.object_key "
        "FROM stickers s JOIN files f ON f.id = s.file_id "
        "ORDER BY s.id",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value s;
                s["id"]    = Json::Int64(row["id"].as<long long>());
                s["label"] = row["label"].as<std::string>();
                std::string url = presignedUrl(
                    row["bucket"].as<std::string>(),
                    row["object_key"].as<std::string>());
                s["url"] = url.empty() ? Json::Value() : Json::Value(url);
                arr.append(s);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listStickers: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        });
}

// GET /stickers/{id}/image — 302 redirect to presigned sticker image URL
void StickersController::getStickerImage(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                          long long stickerId) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT f.bucket, f.object_key FROM stickers s JOIN files f ON f.id = s.file_id WHERE s.id = $1",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("Sticker not found", drogon::k404NotFound));
            std::string url = presignedUrl(
                r[0]["bucket"].as<std::string>(),
                r[0]["object_key"].as<std::string>());
            if (url.empty()) return cb(jsonErr("Sticker image unavailable", drogon::k404NotFound));
            cb(drogon::HttpResponse::newRedirectionResponse(url));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getStickerImage: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, stickerId);
}
