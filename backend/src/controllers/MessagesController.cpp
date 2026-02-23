#include "MessagesController.h"
#include "../config/Config.h"
#include "../utils/MinioPresign.h"
#include "../ws/WsHandler.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

// Presign helper using Config singleton
static std::string presign(const std::string& bucket, const std::string& key) {
    if (bucket.empty() || key.empty()) return "";
    const auto& cfg = Config::get();
    return minio_presign::generatePresignedUrl(
        cfg.minioEndpoint, cfg.minioPublicUrl,
        bucket, key,
        cfg.minioAccessKey, cfg.minioSecretKey, cfg.presignTtl);
}

static void requireMember(long long chatId, long long userId,
                           std::function<void(bool)> cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT 1 FROM chat_members WHERE chat_id = $1 AND user_id = $2",
        [cb](const drogon::orm::Result& r) { cb(!r.empty()); },
        [cb](const drogon::orm::DrogonDbException&) { cb(false); },
        chatId, userId);
}

// Build a JSON message object from an enriched query row
static Json::Value buildMsgJson(const drogon::orm::Row& row) {
    Json::Value msg;
    msg["id"]           = Json::Int64(row["id"].as<long long>());
    msg["chat_id"]      = Json::Int64(row["chat_id"].as<long long>());
    msg["sender_id"]    = Json::Int64(row["sender_id"].as<long long>());
    msg["content"]      = row["content"].isNull() ? Json::Value() : Json::Value(row["content"].as<std::string>());
    msg["message_type"] = row["message_type"].as<std::string>();
    msg["created_at"]   = row["created_at"].as<std::string>();

    msg["sender_username"]     = row["sender_username"].isNull() ? Json::Value() : Json::Value(row["sender_username"].as<std::string>());
    msg["sender_display_name"] = row["sender_display_name"].isNull() ? Json::Value() : Json::Value(row["sender_display_name"].as<std::string>());

    // Sender avatar
    std::string avBucket = row["sender_avatar_bucket"].isNull() ? "" : row["sender_avatar_bucket"].as<std::string>();
    std::string avKey    = row["sender_avatar_key"].isNull()    ? "" : row["sender_avatar_key"].as<std::string>();
    std::string avUrl    = presign(avBucket, avKey);
    msg["sender_avatar_url"] = avUrl.empty() ? Json::Value() : Json::Value(avUrl);

    // Sticker fields
    std::string stickerBucket = row["sticker_bucket"].isNull() ? "" : row["sticker_bucket"].as<std::string>();
    std::string stickerKey    = row["sticker_key"].isNull()    ? "" : row["sticker_key"].as<std::string>();
    if (!stickerKey.empty()) {
        std::string stickerUrl = presign(stickerBucket, stickerKey);
        msg["sticker_url"]   = stickerUrl.empty() ? Json::Value() : Json::Value(stickerUrl);
        msg["sticker_label"] = row["sticker_label"].isNull() ? Json::Value() : Json::Value(row["sticker_label"].as<std::string>());
    } else {
        msg["sticker_url"]   = Json::Value();
        msg["sticker_label"] = Json::Value();
    }

    // File/voice attachment
    std::string attBucket = row["att_bucket"].isNull() ? "" : row["att_bucket"].as<std::string>();
    std::string attKey    = row["att_key"].isNull()    ? "" : row["att_key"].as<std::string>();
    std::string attUrl    = presign(attBucket, attKey);
    msg["attachment_url"] = attUrl.empty() ? Json::Value() : Json::Value(attUrl);

    // Duration for voice messages
    if (!row["duration_seconds"].isNull()) {
        msg["duration_seconds"] = row["duration_seconds"].as<int>();
    } else {
        msg["duration_seconds"] = Json::Value();
    }

    return msg;
}

// The enriched SELECT used by both list and single-message fetch
static const char* kEnrichedMsgSelect =
    "SELECT m.id, m.chat_id, m.sender_id, m.content, m.message_type, m.created_at, "
    "       m.duration_seconds, "
    "       u.username AS sender_username, "
    "       COALESCE(u.display_name, u.username) AS sender_display_name, "
    "       av.bucket AS sender_avatar_bucket, av.object_key AS sender_avatar_key, "
    "       s.label  AS sticker_label, "
    "       sf.bucket AS sticker_bucket, sf.object_key AS sticker_key, "
    "       af.bucket AS att_bucket,    af.object_key  AS att_key "
    "FROM messages m "
    "JOIN users u ON u.id = m.sender_id "
    "LEFT JOIN files av ON av.id = u.avatar_file_id "
    "LEFT JOIN stickers s  ON s.id = m.sticker_id "
    "LEFT JOIN files sf ON sf.id = s.file_id "
    "LEFT JOIN files af ON af.id = m.file_id ";

// POST /chats/{id}/messages
void MessagesController::sendMessage(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string content      = (*body).get("content", "").asString();
    std::string msgType      = (*body).get("type", "text").asString();
    long long   fileId       = (*body).get("file_id",   Json::Value(0)).asInt64();
    long long   stickerId    = (*body).get("sticker_id", Json::Value(0)).asInt64();
    int         durationSecs = (*body).get("duration_seconds", Json::Value(0)).asInt();

    if (content.empty() && fileId == 0 && stickerId == 0)
        return cb(jsonErr("content, file_id or sticker_id required", drogon::k400BadRequest));

    requireMember(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        auto db0 = drogon::app().getDbClient();
        db0->execSqlAsync(
            "SELECT c.type, cm.role FROM chats c "
            "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
            "WHERE c.id = $1",
            [=, cb = std::move(cb)](const drogon::orm::Result& pr) mutable {
                if (!pr.empty()) {
                    std::string chatType = pr[0]["type"].as<std::string>();
                    std::string role     = pr[0]["role"].as<std::string>();
                    if (chatType == "channel" && role != "owner" && role != "admin")
                        return cb(jsonErr("Only admins can post in channels", drogon::k403Forbidden));
                }

                auto doInsert = [=, cb = std::move(cb)](long long resolvedStickerId,
                                                         long long resolvedFileId) mutable {
                    auto db = drogon::app().getDbClient();
                    std::string sql;
                    // Build SQL dynamically based on what fields we have
                    if (resolvedStickerId > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, sticker_id) "
                              "VALUES ($1, $2, $3, $4, $5) RETURNING id, created_at";
                    } else if (resolvedFileId > 0 && durationSecs > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id, duration_seconds) "
                              "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id, created_at";
                    } else if (resolvedFileId > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id) "
                              "VALUES ($1, $2, $3, $4, $5) RETURNING id, created_at";
                    } else {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type) "
                              "VALUES ($1, $2, $3, $4) RETURNING id, created_at";
                    }

                    auto onInserted = [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
                        long long msgId    = r[0]["id"].as<long long>();
                        std::string createdAt = r[0]["created_at"].as<std::string>();

                        auto db2 = drogon::app().getDbClient();
                        db2->execSqlAsync(
                            "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                            [](const drogon::orm::Result&) {},
                            [](const drogon::orm::DrogonDbException& e) {
                                LOG_WARN << "chat updated_at: " << e.base().what();
                            }, chatId);

                        // WS fan-out
                        Json::Value wsMsg;
                        wsMsg["type"]         = "message";
                        wsMsg["id"]           = Json::Int64(msgId);
                        wsMsg["chat_id"]      = Json::Int64(chatId);
                        wsMsg["sender_id"]    = Json::Int64(me);
                        wsMsg["content"]      = content;
                        wsMsg["message_type"] = msgType;
                        wsMsg["created_at"]   = createdAt;
                        WsDispatch::publishMessage(chatId, wsMsg);

                        // Return minimal response; client-side uses S.me for sender info
                        Json::Value resp;
                        resp["id"]           = Json::Int64(msgId);
                        resp["chat_id"]      = Json::Int64(chatId);
                        resp["sender_id"]    = Json::Int64(me);
                        resp["content"]      = content;
                        resp["message_type"] = msgType;
                        resp["type"]         = msgType;
                        resp["created_at"]   = createdAt;
                        if (resolvedStickerId > 0)
                            resp["sticker_id"] = Json::Int64(resolvedStickerId);
                        if (durationSecs > 0)
                            resp["duration_seconds"] = durationSecs;
                        auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
                        httpResp->setStatusCode(drogon::k201Created);
                        cb(httpResp);
                    };

                    auto onErr = [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                        LOG_ERROR << "sendMessage insert: " << e.base().what();
                        cb(jsonErr("Internal error", drogon::k500InternalServerError));
                    };

                    if (resolvedStickerId > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedStickerId);
                    else if (resolvedFileId > 0 && durationSecs > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedFileId, (long long)durationSecs);
                    else if (resolvedFileId > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedFileId);
                    else
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType);
                };

                // For sticker messages, verify sticker exists
                if (stickerId > 0) {
                    auto dbS = drogon::app().getDbClient();
                    dbS->execSqlAsync(
                        "SELECT id FROM stickers WHERE id = $1",
                        [=, cb = std::move(cb), doInsert = std::move(doInsert)](const drogon::orm::Result& sr) mutable {
                            if (sr.empty()) return cb(jsonErr("Sticker not found", drogon::k404NotFound));
                            doInsert(stickerId, 0);
                        },
                        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                            LOG_ERROR << "sticker lookup: " << e.base().what();
                            cb(jsonErr("Internal error", drogon::k500InternalServerError));
                        }, stickerId);
                } else {
                    doInsert(0, fileId);
                }
            },
            [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "channel permission check: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            chatId, me);
    });
}

// GET /chats/{id}/messages?limit=50&before=<id>&after_id=<id>
void MessagesController::listMessages(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                       long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    requireMember(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        long long limit = 50;
        {
            std::string lp = req->getParameter("limit");
            if (!lp.empty()) {
                int l = std::stoi(lp);
                limit = static_cast<long long>(std::max(1, std::min(l, 100)));
            }
        }

        std::string afterStr  = req->getParameter("after_id");
        std::string beforeStr = req->getParameter("before");

        auto db = drogon::app().getDbClient();

        auto handleRows = [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r)
                arr.append(buildMsgJson(row));
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        };

        auto onErr = [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listMessages: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        };

        if (!afterStr.empty()) {
            // Polling: return messages AFTER this id, oldest-first
            long long afterId = std::stoll(afterStr);
            std::string sql = std::string(kEnrichedMsgSelect) +
                "WHERE m.chat_id = $1 AND m.id > $2 "
                "ORDER BY m.created_at ASC LIMIT $3";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             chatId, afterId, limit);
        } else if (!beforeStr.empty()) {
            // Older messages pagination: return N messages BEFORE this id, oldest-first
            long long before = std::stoll(beforeStr);
            std::string sql = std::string(
                "SELECT * FROM (") + kEnrichedMsgSelect +
                "WHERE m.chat_id = $1 AND m.id < $2 "
                "ORDER BY m.created_at DESC LIMIT $3) sub "
                "ORDER BY created_at ASC";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             chatId, before, limit);
        } else {
            // Initial load: latest N messages in chronological order
            std::string sql = std::string(
                "SELECT * FROM (") + kEnrichedMsgSelect +
                "WHERE m.chat_id = $1 "
                "ORDER BY m.created_at DESC LIMIT $2) sub "
                "ORDER BY created_at ASC";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             chatId, limit);
        }
    });
}
