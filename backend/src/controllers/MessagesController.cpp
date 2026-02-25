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
    msg["is_edited"]    = row["is_edited"].as<bool>();
    if (!row["updated_at"].isNull())
        msg["updated_at"] = row["updated_at"].as<std::string>();

    msg["sender_username"]     = row["sender_username"].isNull() ? Json::Value() : Json::Value(row["sender_username"].as<std::string>());
    msg["sender_display_name"] = row["sender_display_name"].isNull() ? Json::Value() : Json::Value(row["sender_display_name"].as<std::string>());
    msg["sender_is_admin"]     = row["sender_is_admin"].isNull() ? false : row["sender_is_admin"].as<bool>();

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

    // Forwarded-from fields
    if (!row["forwarded_from_chat_id"].isNull()) {
        msg["forwarded_from_chat_id"] = Json::Int64(row["forwarded_from_chat_id"].as<long long>());
    } else {
        msg["forwarded_from_chat_id"] = Json::Value();
    }
    if (!row["forwarded_from_message_id"].isNull()) {
        msg["forwarded_from_message_id"] = Json::Int64(row["forwarded_from_message_id"].as<long long>());
    } else {
        msg["forwarded_from_message_id"] = Json::Value();
    }
    if (!row["forwarded_from_user_id"].isNull()) {
        msg["forwarded_from_user_id"] = Json::Int64(row["forwarded_from_user_id"].as<long long>());
    } else {
        msg["forwarded_from_user_id"] = Json::Value();
    }
    if (!row["forwarded_from_display_name"].isNull()) {
        msg["forwarded_from_display_name"] = row["forwarded_from_display_name"].as<std::string>();
    } else {
        msg["forwarded_from_display_name"] = Json::Value();
    }

    // Reply-to fields
    if (!row["reply_to_message_id"].isNull()) {
        msg["reply_to_message_id"] = Json::Int64(row["reply_to_message_id"].as<long long>());
        msg["reply_to_content"] = row["reply_to_content"].isNull() ? Json::Value() : Json::Value(row["reply_to_content"].as<std::string>());
        msg["reply_to_type"] = row["reply_to_type"].isNull() ? Json::Value() : Json::Value(row["reply_to_type"].as<std::string>());
        msg["reply_to_sender_username"] = row["reply_to_sender_username"].isNull() ? Json::Value() : Json::Value(row["reply_to_sender_username"].as<std::string>());
        msg["reply_to_sender_name"] = row["reply_to_sender_name"].isNull() ? Json::Value() : Json::Value(row["reply_to_sender_name"].as<std::string>());
    } else {
        msg["reply_to_message_id"] = Json::Value();
    }

    return msg;
}

// The enriched SELECT used by both list and single-message fetch
static const char* kEnrichedMsgSelect =
    "SELECT m.id, m.chat_id, m.sender_id, m.content, m.message_type, m.created_at, "
    "       m.is_edited, m.updated_at, "
    "       m.duration_seconds, "
    "       m.forwarded_from_chat_id, m.forwarded_from_message_id, "
    "       m.forwarded_from_user_id, m.forwarded_from_display_name, "
    "       m.reply_to_message_id, "
    "       u.username AS sender_username, "
    "       COALESCE(u.display_name, u.username) AS sender_display_name, "
    "       u.is_admin AS sender_is_admin, "
    "       av.bucket AS sender_avatar_bucket, av.object_key AS sender_avatar_key, "
    "       s.label  AS sticker_label, "
    "       sf.bucket AS sticker_bucket, sf.object_key AS sticker_key, "
    "       af.bucket AS att_bucket,    af.object_key  AS att_key, "
    "       rm.content AS reply_to_content, "
    "       rm.message_type AS reply_to_type, "
    "       ru.username AS reply_to_sender_username, "
    "       COALESCE(ru.display_name, ru.username) AS reply_to_sender_name "
    "FROM messages m "
    "JOIN users u ON u.id = m.sender_id "
    "LEFT JOIN files av ON av.id = u.avatar_file_id "
    "LEFT JOIN stickers s  ON s.id = m.sticker_id "
    "LEFT JOIN files sf ON sf.id = s.file_id "
    "LEFT JOIN files af ON af.id = m.file_id "
    "LEFT JOIN messages rm ON rm.id = m.reply_to_message_id "
    "LEFT JOIN users ru ON ru.id = rm.sender_id ";

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
    long long   replyToMsgId = (*body).get("reply_to_message_id", Json::Value(0)).asInt64();

    if (content.empty() && fileId == 0 && stickerId == 0)
        return cb(jsonErr("content, file_id or sticker_id required", drogon::k400BadRequest));

    // Use shared_ptr so cb can be safely shared across all async callbacks
    // without triggering bad_function_call from double-move.
    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=, cbPtr](bool isMember) mutable {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        auto db0 = drogon::app().getDbClient();
        db0->execSqlAsync(
            "SELECT c.type, cm.role FROM chats c "
            "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
            "WHERE c.id = $1",
            [=, cbPtr](const drogon::orm::Result& pr) mutable {
                if (!pr.empty()) {
                    std::string chatType = pr[0]["type"].as<std::string>();
                    std::string role     = pr[0]["role"].as<std::string>();
                    if (chatType == "channel" && role != "owner" && role != "admin")
                        return (*cbPtr)(jsonErr("Only admins can post in channels", drogon::k403Forbidden));
                }

                auto doInsert = [=, cbPtr](long long resolvedStickerId,
                                           long long resolvedFileId) mutable {
                    auto db = drogon::app().getDbClient();
                    std::string sql;
                    if (resolvedStickerId > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, sticker_id, reply_to_message_id) "
                              "VALUES ($1, $2, $3, $4, $5, NULLIF($6::BIGINT, 0)) RETURNING id, created_at";
                    } else if (resolvedFileId > 0 && durationSecs > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id, duration_seconds, reply_to_message_id) "
                              "VALUES ($1, $2, $3, $4, $5, $6, NULLIF($7::BIGINT, 0)) RETURNING id, created_at";
                    } else if (resolvedFileId > 0) {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id, reply_to_message_id) "
                              "VALUES ($1, $2, $3, $4, $5, NULLIF($6::BIGINT, 0)) RETURNING id, created_at";
                    } else {
                        sql = "INSERT INTO messages (chat_id, sender_id, content, message_type, reply_to_message_id) "
                              "VALUES ($1, $2, $3, $4, NULLIF($5::BIGINT, 0)) RETURNING id, created_at";
                    }

                    auto onInserted = [=, cbPtr](const drogon::orm::Result& r) mutable {
                        long long msgId       = r[0]["id"].as<long long>();
                        std::string createdAt = r[0]["created_at"].as<std::string>();

                        auto db2 = drogon::app().getDbClient();
                        db2->execSqlAsync(
                            "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                            [](const drogon::orm::Result&) {},
                            [](const drogon::orm::DrogonDbException& e) {
                                LOG_WARN << "chat updated_at: " << e.base().what();
                            }, chatId);

                        // Auto-mark sender's own message as read
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "INSERT INTO chat_last_read (user_id, chat_id, last_read_msg_id, read_at) "
                            "VALUES ($1, $2, $3, NOW()) "
                            "ON CONFLICT (user_id, chat_id) DO UPDATE SET "
                            "  last_read_msg_id = GREATEST(chat_last_read.last_read_msg_id, EXCLUDED.last_read_msg_id), "
                            "  read_at = NOW()",
                            [](const drogon::orm::Result&) {},
                            [](const drogon::orm::DrogonDbException& e) {
                                LOG_WARN << "auto-mark-read on send: " << e.base().what();
                            }, me, chatId, msgId);

                        Json::Value wsMsg;
                        wsMsg["type"]         = "message";
                        wsMsg["id"]           = Json::Int64(msgId);
                        wsMsg["chat_id"]      = Json::Int64(chatId);
                        wsMsg["sender_id"]    = Json::Int64(me);
                        wsMsg["content"]      = content;
                        wsMsg["message_type"] = msgType;
                        wsMsg["created_at"]   = createdAt;
                        if (replyToMsgId > 0)
                            wsMsg["reply_to_message_id"] = Json::Int64(replyToMsgId);
                        WsDispatch::publishMessage(chatId, wsMsg);

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
                        if (replyToMsgId > 0)
                            resp["reply_to_message_id"] = Json::Int64(replyToMsgId);
                        auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
                        httpResp->setStatusCode(drogon::k201Created);
                        (*cbPtr)(httpResp);
                    };

                    auto onErr = [cbPtr](const drogon::orm::DrogonDbException& e) mutable {
                        LOG_ERROR << "sendMessage insert: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    };

                    if (resolvedStickerId > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedStickerId, replyToMsgId);
                    else if (resolvedFileId > 0 && durationSecs > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedFileId, durationSecs, replyToMsgId);
                    else if (resolvedFileId > 0)
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, resolvedFileId, replyToMsgId);
                    else
                        db->execSqlAsync(sql, std::move(onInserted), std::move(onErr),
                                         chatId, me, content, msgType, replyToMsgId);
                };

                if (stickerId > 0) {
                    auto dbS = drogon::app().getDbClient();
                    dbS->execSqlAsync(
                        "SELECT id FROM stickers WHERE id = $1",
                        [=, cbPtr, doInsert = std::move(doInsert)](const drogon::orm::Result& sr) mutable {
                            if (sr.empty()) return (*cbPtr)(jsonErr("Sticker not found", drogon::k404NotFound));
                            doInsert(stickerId, 0);
                        },
                        [cbPtr](const drogon::orm::DrogonDbException& e) mutable {
                            LOG_ERROR << "sticker lookup: " << e.base().what();
                            (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                        }, stickerId);
                } else {
                    doInsert(0, fileId);
                }
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "channel permission check: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
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

        // Soft-delete and per-user delete filter fragment
        const std::string deletedFilter =
            "AND m.is_deleted = FALSE "
            "AND NOT EXISTS (SELECT 1 FROM deleted_messages dm WHERE dm.message_id = m.id AND dm.user_id = $1) ";

        if (!afterStr.empty()) {
            // Polling: return messages AFTER this id, oldest-first
            long long afterId = std::stoll(afterStr);
            std::string sql = std::string(kEnrichedMsgSelect) +
                "WHERE m.chat_id = $2 AND m.id > $3 " + deletedFilter +
                "ORDER BY m.created_at ASC LIMIT $4";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             me, chatId, afterId, limit);
        } else if (!beforeStr.empty()) {
            // Older messages pagination: return N messages BEFORE this id, oldest-first
            long long before = std::stoll(beforeStr);
            std::string sql = std::string(
                "SELECT * FROM (") + kEnrichedMsgSelect +
                "WHERE m.chat_id = $2 AND m.id < $3 " + deletedFilter +
                "ORDER BY m.created_at DESC LIMIT $4) sub "
                "ORDER BY created_at ASC";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             me, chatId, before, limit);
        } else {
            // Initial load: latest N messages in chronological order
            std::string sql = std::string(
                "SELECT * FROM (") + kEnrichedMsgSelect +
                "WHERE m.chat_id = $2 " + deletedFilter +
                "ORDER BY m.created_at DESC LIMIT $3) sub "
                "ORDER BY created_at ASC";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             me, chatId, limit);
        }
    });
}

// DELETE /chats/{chatId}/messages/{messageId}
// Body (optional): { "for_everyone": true }
void MessagesController::deleteMessage(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                        long long chatId, long long messageId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    bool forEveryone = false;
    auto body = req->getJsonObject();
    if (body) {
        forEveryone = (*body).get("for_everyone", false).asBool();
    }

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Fetch message details + user's role in chat
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT m.sender_id, m.created_at, cm.role "
            "FROM messages m "
            "JOIN chat_members cm ON cm.chat_id = m.chat_id AND cm.user_id = $3 "
            "WHERE m.id = $1 AND m.chat_id = $2 AND m.is_deleted = false",
            [=](const drogon::orm::Result& r) {
                if (r.empty())
                    return (*cbPtr)(jsonErr("Message not found", drogon::k404NotFound));

                long long senderId = r[0]["sender_id"].as<long long>();
                std::string role   = r[0]["role"].as<std::string>();
                std::string createdAtStr = r[0]["created_at"].as<std::string>();

                if (!forEveryone) {
                    // Delete for me only: insert into deleted_messages
                    auto db2 = drogon::app().getDbClient();
                    db2->execSqlAsync(
                        "INSERT INTO deleted_messages (user_id, message_id) "
                        "VALUES ($1, $2) ON CONFLICT DO NOTHING",
                        [cbPtr](const drogon::orm::Result&) {
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k204NoContent);
                            (*cbPtr)(resp);
                        },
                        [cbPtr](const drogon::orm::DrogonDbException& e) {
                            LOG_ERROR << "deleteMessage (for me): " << e.base().what();
                            (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                        },
                        me, messageId);
                    return;
                }

                // Delete for everyone: check permissions
                bool isSender = (senderId == me);
                bool isAdmin  = (role == "owner" || role == "admin");
                if (!isSender && !isAdmin)
                    return (*cbPtr)(jsonErr("Only sender or admin can delete for everyone", drogon::k403Forbidden));

                // Check 48h time window
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT ($1::timestamptz > NOW() - INTERVAL '48 hours') AS within_window",
                    [=](const drogon::orm::Result& tr) {
                        bool withinWindow = tr[0]["within_window"].as<bool>();
                        if (!withinWindow)
                            return (*cbPtr)(jsonErr("Cannot delete for everyone after 48 hours", drogon::k403Forbidden));

                        // Set is_deleted = true
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "UPDATE messages SET is_deleted = true WHERE id = $1 AND chat_id = $2",
                            [=](const drogon::orm::Result&) {
                                // Broadcast via WebSocket
                                Json::Value wsPayload;
                                wsPayload["type"]         = "message_deleted";
                                wsPayload["chat_id"]      = Json::Int64(chatId);
                                wsPayload["message_id"]   = Json::Int64(messageId);
                                wsPayload["deleted_by"]   = Json::Int64(me);
                                wsPayload["for_everyone"] = true;
                                WsDispatch::publishMessage(chatId, wsPayload);

                                auto resp = drogon::HttpResponse::newHttpResponse();
                                resp->setStatusCode(drogon::k204NoContent);
                                (*cbPtr)(resp);
                            },
                            [cbPtr](const drogon::orm::DrogonDbException& e) {
                                LOG_ERROR << "deleteMessage (for everyone): " << e.base().what();
                                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                            },
                            messageId, chatId);
                    },
                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "deleteMessage time check: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    },
                    createdAtStr);
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "deleteMessage lookup: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            messageId, chatId, me);
    });
}

// PUT /chats/{chatId}/messages/{messageId}
void MessagesController::editMessage(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      long long chatId, long long messageId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string content = (*body).get("content", "").asString();
    if (content.empty())
        return cb(jsonErr("content required", drogon::k400BadRequest));
    if (content.size() > 4000)
        return cb(jsonErr("content too long (max 4000)", drogon::k400BadRequest));

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "UPDATE messages SET content = $1, is_edited = TRUE, updated_at = NOW() "
            "WHERE id = $2 AND chat_id = $3 AND sender_id = $4 AND message_type = 'text' "
            "AND is_deleted = FALSE "
            "RETURNING id, content, updated_at",
            [=](const drogon::orm::Result& r) {
                if (r.empty())
                    return (*cbPtr)(jsonErr("Message not found or not editable", drogon::k403Forbidden));

                std::string updatedContent = r[0]["content"].as<std::string>();
                std::string updatedAt      = r[0]["updated_at"].as<std::string>();

                // Broadcast via WebSocket
                Json::Value wsPayload;
                wsPayload["type"]       = "message_updated";
                wsPayload["chat_id"]    = Json::Int64(chatId);
                wsPayload["message_id"] = Json::Int64(messageId);
                wsPayload["content"]    = updatedContent;
                wsPayload["updated_at"] = updatedAt;
                WsDispatch::publishMessage(chatId, wsPayload);

                Json::Value resp;
                resp["id"]        = Json::Int64(messageId);
                resp["content"]   = updatedContent;
                resp["is_edited"] = true;
                resp["updated_at"]= updatedAt;
                (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "editMessage: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            content, messageId, chatId, me);
    });
}

// POST /chats/{chatId}/messages/{messageId}/pin
void MessagesController::pinMessage(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                     long long chatId, long long messageId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Check channel permission: only owner/admin can pin in channels
        auto db0 = drogon::app().getDbClient();
        db0->execSqlAsync(
            "SELECT c.type, cm.role FROM chats c "
            "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
            "WHERE c.id = $1",
            [=](const drogon::orm::Result& pr) {
                if (!pr.empty()) {
                    std::string chatType = pr[0]["type"].as<std::string>();
                    std::string role     = pr[0]["role"].as<std::string>();
                    if (chatType == "channel" && role != "owner" && role != "admin")
                        return (*cbPtr)(jsonErr("Only admins can pin in channels", drogon::k403Forbidden));
                }

                // Verify message exists and belongs to this chat
                auto db1 = drogon::app().getDbClient();
                db1->execSqlAsync(
                    "SELECT id FROM messages WHERE id = $1 AND chat_id = $2 AND is_deleted = FALSE",
                    [=](const drogon::orm::Result& mr) {
                        if (mr.empty())
                            return (*cbPtr)(jsonErr("Message not found", drogon::k404NotFound));

                        // Unpin current pinned message (if any)
                        auto db2 = drogon::app().getDbClient();
                        db2->execSqlAsync(
                            "UPDATE pinned_messages SET unpinned_at = NOW() WHERE chat_id = $1 AND unpinned_at IS NULL",
                            [=](const drogon::orm::Result&) {
                                // Insert new pin
                                auto db3 = drogon::app().getDbClient();
                                db3->execSqlAsync(
                                    "INSERT INTO pinned_messages (chat_id, message_id, pinned_by) "
                                    "VALUES ($1, $2, $3) RETURNING id, pinned_at",
                                    [=](const drogon::orm::Result& ir) {
                                        std::string pinnedAt = ir[0]["pinned_at"].as<std::string>();

                                        // Fetch enriched message
                                        auto db4 = drogon::app().getDbClient();
                                        std::string sql = std::string(kEnrichedMsgSelect) + "WHERE m.id = $1";
                                        db4->execSqlAsync(sql,
                                            [=](const drogon::orm::Result& er) {
                                                Json::Value msgJson;
                                                if (!er.empty()) msgJson = buildMsgJson(er[0]);

                                                // WS broadcast
                                                Json::Value wsPayload;
                                                wsPayload["type"]       = "message_pinned";
                                                wsPayload["chat_id"]    = Json::Int64(chatId);
                                                wsPayload["message_id"] = Json::Int64(messageId);
                                                wsPayload["pinned_by"]  = Json::Int64(me);
                                                wsPayload["message"]    = msgJson;
                                                WsDispatch::publishMessage(chatId, wsPayload);

                                                // HTTP response
                                                Json::Value resp;
                                                resp["pinned_message_id"] = Json::Int64(messageId);
                                                resp["message"]    = msgJson;
                                                resp["pinned_at"]  = pinnedAt;
                                                resp["pinned_by"]  = Json::Int64(me);
                                                (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
                                            },
                                            [cbPtr](const drogon::orm::DrogonDbException& e) {
                                                LOG_ERROR << "pinMessage enriched fetch: " << e.base().what();
                                                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                                            },
                                            messageId);
                                    },
                                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                                        LOG_ERROR << "pinMessage insert: " << e.base().what();
                                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                                    },
                                    chatId, messageId, me);
                            },
                            [cbPtr](const drogon::orm::DrogonDbException& e) {
                                LOG_ERROR << "pinMessage unpin current: " << e.base().what();
                                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                            },
                            chatId);
                    },
                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "pinMessage msg lookup: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    },
                    messageId, chatId);
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "pinMessage permission check: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            chatId, me);
    });
}

// DELETE /chats/{chatId}/messages/{messageId}/pin
void MessagesController::unpinMessage(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                       long long chatId, long long messageId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Check channel permission
        auto db0 = drogon::app().getDbClient();
        db0->execSqlAsync(
            "SELECT c.type, cm.role FROM chats c "
            "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
            "WHERE c.id = $1",
            [=](const drogon::orm::Result& pr) {
                if (!pr.empty()) {
                    std::string chatType = pr[0]["type"].as<std::string>();
                    std::string role     = pr[0]["role"].as<std::string>();
                    if (chatType == "channel" && role != "owner" && role != "admin")
                        return (*cbPtr)(jsonErr("Only admins can unpin in channels", drogon::k403Forbidden));
                }

                auto db1 = drogon::app().getDbClient();
                db1->execSqlAsync(
                    "UPDATE pinned_messages SET unpinned_at = NOW() "
                    "WHERE chat_id = $1 AND message_id = $2 AND unpinned_at IS NULL "
                    "RETURNING id",
                    [=](const drogon::orm::Result& r) {
                        if (r.empty())
                            return (*cbPtr)(jsonErr("No pinned message found", drogon::k404NotFound));

                        // WS broadcast
                        Json::Value wsPayload;
                        wsPayload["type"]       = "message_unpinned";
                        wsPayload["chat_id"]    = Json::Int64(chatId);
                        wsPayload["message_id"] = Json::Int64(messageId);
                        WsDispatch::publishMessage(chatId, wsPayload);

                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k204NoContent);
                        (*cbPtr)(resp);
                    },
                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "unpinMessage: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    },
                    chatId, messageId);
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "unpinMessage permission check: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            chatId, me);
    });
}

// GET /chats/{chatId}/pinned-message
void MessagesController::getPinnedMessage(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                           long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT pm.message_id, pm.pinned_by, pm.pinned_at "
            "FROM pinned_messages pm "
            "WHERE pm.chat_id = $1 AND pm.unpinned_at IS NULL "
            "LIMIT 1",
            [=](const drogon::orm::Result& r) {
                if (r.empty()) {
                    Json::Value resp(Json::nullValue);
                    (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
                    return;
                }

                long long msgId    = r[0]["message_id"].as<long long>();
                long long pinnedBy = r[0]["pinned_by"].as<long long>();
                std::string pinnedAt = r[0]["pinned_at"].as<std::string>();

                // Fetch enriched message
                auto db2 = drogon::app().getDbClient();
                std::string sql = std::string(kEnrichedMsgSelect) + "WHERE m.id = $1";
                db2->execSqlAsync(sql,
                    [=](const drogon::orm::Result& er) {
                        Json::Value resp;
                        if (!er.empty()) {
                            resp["message"]   = buildMsgJson(er[0]);
                        } else {
                            resp["message"]   = Json::Value(Json::nullValue);
                        }
                        resp["pinned_at"] = pinnedAt;
                        resp["pinned_by"] = Json::Int64(pinnedBy);
                        (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
                    },
                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "getPinnedMessage enriched fetch: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    },
                    msgId);
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "getPinnedMessage: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            chatId);
    });
}

// GET /chats/{id}/messages/search?q=text&limit=20&before_id=N
void MessagesController::searchMessages(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                         long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    std::string q = req->getParameter("q");
    if (q.empty())
        return cb(jsonErr("q parameter required", drogon::k400BadRequest));
    if (q.size() > 200)
        return cb(jsonErr("q too long (max 200)", drogon::k400BadRequest));

    long long limit = 20;
    {
        std::string lp = req->getParameter("limit");
        if (!lp.empty()) {
            int l = std::stoi(lp);
            limit = static_cast<long long>(std::max(1, std::min(l, 50)));
        }
    }

    std::string beforeIdStr = req->getParameter("before_id");

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        std::string searchPattern = "%" + q + "%";

        const std::string deletedFilter =
            "AND m.is_deleted = FALSE "
            "AND NOT EXISTS (SELECT 1 FROM deleted_messages dm WHERE dm.message_id = m.id AND dm.user_id = $1) ";

        auto handleRows = [cbPtr](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r)
                arr.append(buildMsgJson(row));
            (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(arr));
        };

        auto onErr = [cbPtr](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "searchMessages: " << e.base().what();
            (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
        };

        auto db = drogon::app().getDbClient();

        if (!beforeIdStr.empty()) {
            long long beforeId = std::stoll(beforeIdStr);
            std::string sql = std::string(kEnrichedMsgSelect) +
                "WHERE m.chat_id = $2 AND m.message_type = 'text' AND m.content ILIKE $3 "
                "AND m.id < $4 " + deletedFilter +
                "ORDER BY m.created_at DESC LIMIT $5";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             me, chatId, searchPattern, beforeId, limit);
        } else {
            std::string sql = std::string(kEnrichedMsgSelect) +
                "WHERE m.chat_id = $2 AND m.message_type = 'text' AND m.content ILIKE $3 " + deletedFilter +
                "ORDER BY m.created_at DESC LIMIT $4";
            db->execSqlAsync(sql, std::move(handleRows), std::move(onErr),
                             me, chatId, searchPattern, limit);
        }
    });
}

// POST /chats/{targetChatId}/forward
void MessagesController::forwardMessages(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                          long long targetChatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    long long fromChatId = (*body).get("from_chat_id", Json::Value(0)).asInt64();
    if (fromChatId <= 0)
        return cb(jsonErr("from_chat_id required", drogon::k400BadRequest));

    auto& msgIdsVal = (*body)["message_ids"];
    if (!msgIdsVal.isArray() || msgIdsVal.empty())
        return cb(jsonErr("message_ids array required", drogon::k400BadRequest));
    if (msgIdsVal.size() > 100)
        return cb(jsonErr("Maximum 100 messages per forward", drogon::k400BadRequest));

    std::vector<long long> messageIds;
    messageIds.reserve(msgIdsVal.size());
    for (const auto& v : msgIdsVal) {
        messageIds.push_back(v.asInt64());
    }

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    // Check membership in source chat
    requireMember(fromChatId, me, [=](bool isMemberSource) {
        if (!isMemberSource)
            return (*cbPtr)(jsonErr("Not a member of source chat", drogon::k403Forbidden));

        // Check membership in target chat
        requireMember(targetChatId, me, [=](bool isMemberTarget) {
            if (!isMemberTarget)
                return (*cbPtr)(jsonErr("Not a member of target chat", drogon::k403Forbidden));

            // Build IN clause for message IDs
            std::string inClause;
            for (size_t i = 0; i < messageIds.size(); ++i) {
                if (i > 0) inClause += ",";
                inClause += std::to_string(messageIds[i]);
            }

            // Fetch original messages with sender info
            auto db = drogon::app().getDbClient();
            std::string fetchSql =
                "SELECT m.id, m.content, m.message_type, m.sender_id, m.file_id, m.sticker_id, m.duration_seconds, "
                "       COALESCE(u.display_name, u.username) AS orig_sender_name "
                "FROM messages m JOIN users u ON u.id = m.sender_id "
                "WHERE m.id IN (" + inClause + ") AND m.chat_id = $1 AND m.is_deleted = false "
                "ORDER BY m.id ASC";

            db->execSqlAsync(fetchSql,
                [=](const drogon::orm::Result& origRows) {
                    if (origRows.empty())
                        return (*cbPtr)(jsonErr("No valid messages found", drogon::k404NotFound));

                    // Build bulk insert for forwarded messages
                    std::string insertSql =
                        "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id, sticker_id, "
                        "duration_seconds, forwarded_from_chat_id, forwarded_from_message_id) VALUES ";

                    std::vector<std::string> valueSets;
                    int paramIdx = 1;
                    // We'll build parameterized values manually using literal embedding
                    // for safety with the async API. Use a CTE approach instead.

                    // Actually, build a single multi-row INSERT with literal values
                    // is risky. Let's use a simpler approach: iterate and insert one by one,
                    // collecting results.

                    auto newMessages = std::make_shared<Json::Value>(Json::arrayValue);
                    auto remaining = std::make_shared<int>(static_cast<int>(origRows.size()));
                    auto hasError = std::make_shared<bool>(false);

                    for (const auto& row : origRows) {
                        long long origId = row["id"].as<long long>();
                        std::string content = row["content"].isNull() ? "" : row["content"].as<std::string>();
                        std::string msgType = row["message_type"].as<std::string>();
                        long long fileId = row["file_id"].isNull() ? 0 : row["file_id"].as<long long>();
                        long long stickerId = row["sticker_id"].isNull() ? 0 : row["sticker_id"].as<long long>();
                        int duration = row["duration_seconds"].isNull() ? 0 : row["duration_seconds"].as<int>();
                        long long origSenderId = row["sender_id"].as<long long>();
                        std::string origSenderName = row["orig_sender_name"].isNull() ? "" : row["orig_sender_name"].as<std::string>();

                        auto db2 = drogon::app().getDbClient();
                        db2->execSqlAsync(
                            "INSERT INTO messages (chat_id, sender_id, content, message_type, "
                            "file_id, sticker_id, duration_seconds, "
                            "forwarded_from_chat_id, forwarded_from_message_id, "
                            "forwarded_from_user_id, forwarded_from_display_name) "
                            "VALUES ($1, $2, $3, $4, "
                            "NULLIF($5::BIGINT, 0), NULLIF($6::BIGINT, 0), NULLIF($7::BIGINT, 0), $8, $9, $10, NULLIF($11, '')) "
                            "RETURNING id, created_at",
                            [=](const drogon::orm::Result& ir) {
                                long long newId = ir[0]["id"].as<long long>();
                                std::string createdAt = ir[0]["created_at"].as<std::string>();

                                Json::Value msg;
                                msg["id"] = Json::Int64(newId);
                                msg["chat_id"] = Json::Int64(targetChatId);
                                msg["sender_id"] = Json::Int64(me);
                                msg["content"] = content;
                                msg["message_type"] = msgType;
                                msg["created_at"] = createdAt;
                                msg["forwarded_from_chat_id"] = Json::Int64(fromChatId);
                                msg["forwarded_from_message_id"] = Json::Int64(origId);
                                msg["forwarded_from_user_id"] = Json::Int64(origSenderId);
                                if (!origSenderName.empty())
                                    msg["forwarded_from_display_name"] = origSenderName;

                                // Broadcast via WS
                                Json::Value wsMsg;
                                wsMsg["type"] = "message";
                                wsMsg["id"] = Json::Int64(newId);
                                wsMsg["chat_id"] = Json::Int64(targetChatId);
                                wsMsg["sender_id"] = Json::Int64(me);
                                wsMsg["content"] = content;
                                wsMsg["message_type"] = msgType;
                                wsMsg["created_at"] = createdAt;
                                wsMsg["forwarded_from_chat_id"] = Json::Int64(fromChatId);
                                wsMsg["forwarded_from_message_id"] = Json::Int64(origId);
                                wsMsg["forwarded_from_user_id"] = Json::Int64(origSenderId);
                                if (!origSenderName.empty())
                                    wsMsg["forwarded_from_display_name"] = origSenderName;
                                WsDispatch::publishMessage(targetChatId, wsMsg);

                                newMessages->append(msg);

                                int left = --(*remaining);
                                if (left == 0 && !*hasError) {
                                    // Update target chat updated_at
                                    auto db3 = drogon::app().getDbClient();
                                    db3->execSqlAsync(
                                        "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                                        [](const drogon::orm::Result&) {},
                                        [](const drogon::orm::DrogonDbException& e) {
                                            LOG_WARN << "forward chat updated_at: " << e.base().what();
                                        }, targetChatId);

                                    auto resp = drogon::HttpResponse::newHttpJsonResponse(*newMessages);
                                    resp->setStatusCode(drogon::k200OK);
                                    (*cbPtr)(resp);
                                }
                            },
                            [=](const drogon::orm::DrogonDbException& e) {
                                LOG_ERROR << "forwardMessages insert: " << e.base().what();
                                if (!*hasError) {
                                    *hasError = true;
                                    (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                                }
                            },
                            targetChatId, me, content, msgType,
                            fileId, stickerId, (long long)duration,
                            fromChatId, origId, origSenderId, origSenderName);
                    }
                },
                [cbPtr](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "forwardMessages fetch: " << e.base().what();
                    (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                },
                fromChatId);
        });
    });
}
