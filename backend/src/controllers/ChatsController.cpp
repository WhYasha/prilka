#include "ChatsController.h"
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

static std::string avatarUrl(const std::string& bucket, const std::string& key) {
    if (bucket.empty() || key.empty()) return "";
    const auto& cfg = Config::get();
    return minio_presign::generatePresignedUrl(
        cfg.minioEndpoint, cfg.minioPublicUrl,
        bucket, key,
        cfg.minioAccessKey, cfg.minioSecretKey, cfg.presignTtl);
}

// POST /chats
// Body: { "type": "direct"|"group"|"channel", "name": "...", "title": "...",
//         "description": "...", "public_name": "...", "member_ids": [2, 3] }
void ChatsController::createChat(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string type = (*body).get("type", "direct").asString();
    if (type != "direct" && type != "group" && type != "channel")
        return cb(jsonErr("type must be 'direct', 'group', or 'channel'", drogon::k400BadRequest));

    std::string name = (*body).get("name", "").asString();
    std::string title = (*body).get("title", "").asString();
    std::string description = (*body).get("description", "").asString();
    std::string publicName = (*body).get("public_name", "").asString();

    if (type == "channel") {
        if (title.empty())
            return cb(jsonErr("title is required for channel type", drogon::k400BadRequest));
        name = title;
    }

    auto memberIds = (*body)["member_ids"];
    std::vector<long long> members;
    members.push_back(me);
    for (auto& id : memberIds) {
        long long mid = id.asInt64();
        if (mid != me) members.push_back(mid);
    }

    if (type == "direct" && members.size() != 2)
        return cb(jsonErr("direct chat requires exactly one other member_id", drogon::k400BadRequest));

    // For direct chats: check if DM already exists to avoid duplicates
    if (type == "direct") {
        long long otherId = members[1];
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT c.id FROM chats c "
            "JOIN chat_members cm1 ON cm1.chat_id = c.id AND cm1.user_id = $1 "
            "JOIN chat_members cm2 ON cm2.chat_id = c.id AND cm2.user_id = $2 "
            "WHERE c.type = 'direct' LIMIT 1",
            [cb, type, title, otherId, members, me, name, description, publicName]
            (const drogon::orm::Result& r) mutable {
                if (!r.empty()) {
                    // Return existing DM
                    long long chatId = r[0]["id"].as<long long>();
                    Json::Value resp;
                    resp["id"]   = Json::Int64(chatId);
                    resp["type"] = type;
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                    return;
                }
                // Create new DM
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "INSERT INTO chats (type, name, title, description, public_name, owner_id) "
                    "VALUES ($1, $2, $3, $4, NULLIF($5, ''), $6) RETURNING id",
                    [cb, members, me, type, title](const drogon::orm::Result& r2) mutable {
                        long long chatId = r2[0]["id"].as<long long>();
                        auto db3 = drogon::app().getDbClient();
                        for (auto uid : members) {
                            std::string role = (uid == me) ? "owner" : "member";
                            db3->execSqlAsync(
                                "INSERT INTO chat_members (chat_id, user_id, role) VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                                [](const drogon::orm::Result&) {},
                                [](const drogon::orm::DrogonDbException& e) {
                                    LOG_WARN << "member insert: " << e.base().what();
                                }, chatId, uid, role);
                        }
                        Json::Value resp;
                        resp["id"]   = Json::Int64(chatId);
                        resp["type"] = type;
                        auto respObj = drogon::HttpResponse::newHttpJsonResponse(resp);
                        respObj->setStatusCode(drogon::k201Created);
                        cb(respObj);
                    },
                    [cb](const drogon::orm::DrogonDbException& e) mutable {
                        LOG_ERROR << "createChat direct: " << e.base().what();
                        cb(jsonErr("Internal error", drogon::k500InternalServerError));
                    }, type, name, title, description, publicName, me);
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "DM lookup: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, me, otherId);
        return;
    }

    // Group / Channel creation
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO chats (type, name, title, description, public_name, owner_id) "
        "VALUES ($1, $2, $3, $4, NULLIF($5, ''), $6) RETURNING id",
        [cb, members, me, type, title](const drogon::orm::Result& r) mutable {
            long long chatId = r[0]["id"].as<long long>();
            auto db2 = drogon::app().getDbClient();
            for (auto uid : members) {
                std::string role = (uid == me) ? "owner" : "member";
                db2->execSqlAsync(
                    "INSERT INTO chat_members (chat_id, user_id, role) VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                    [](const drogon::orm::Result&) {},
                    [](const drogon::orm::DrogonDbException& e) {
                        LOG_WARN << "member insert: " << e.base().what();
                    }, chatId, uid, role);
            }
            Json::Value resp;
            resp["id"]   = Json::Int64(chatId);
            resp["type"] = type;
            if (!title.empty()) resp["title"] = title;
            auto respObj = drogon::HttpResponse::newHttpJsonResponse(resp);
            respObj->setStatusCode(drogon::k201Created);
            cb(respObj);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            std::string what = e.base().what();
            if (what.find("unique") != std::string::npos ||
                what.find("duplicate") != std::string::npos)
                return cb(jsonErr("public_name already taken", drogon::k409Conflict));
            LOG_ERROR << "createChat: " << what;
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, type, name, title, description, publicName, me);
}

// GET /chats — with DM enrichment, favorites, mute status
void ChatsController::listChats(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.title, c.description, c.public_name, c.updated_at, "
        "    (SELECT m.content FROM messages m WHERE m.chat_id = c.id ORDER BY m.created_at DESC LIMIT 1) AS last_msg, "
        "    (SELECT m.created_at FROM messages m WHERE m.chat_id = c.id ORDER BY m.created_at DESC LIMIT 1) AS last_msg_at, "
        "    ou.id           AS other_user_id, "
        "    ou.username     AS other_username, "
        "    COALESCE(ou.display_name, ou.username) AS other_display_name, "
        "    ouf.bucket      AS other_avatar_bucket, "
        "    ouf.object_key  AS other_avatar_key, "
        "    (cf.chat_id IS NOT NULL) AS is_favorite, "
        "    (cms.user_id IS NOT NULL) AS is_muted, "
        "    COALESCE(( "
        "        SELECT COUNT(*) FROM messages m2 "
        "        WHERE m2.chat_id = c.id AND m2.id > COALESCE(clr.last_read_msg_id, 0) "
        "    ), 0) AS unread_count "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $1 "
        "LEFT JOIN LATERAL ( "
        "    SELECT u2.id, u2.username, u2.display_name, u2.avatar_file_id "
        "    FROM chat_members cm2 JOIN users u2 ON u2.id = cm2.user_id "
        "    WHERE cm2.chat_id = c.id AND cm2.user_id != $1 AND c.type = 'direct' LIMIT 1 "
        ") ou ON c.type = 'direct' "
        "LEFT JOIN files ouf ON ouf.id = ou.avatar_file_id "
        "LEFT JOIN chat_favorites cf ON cf.chat_id = c.id AND cf.user_id = $1 "
        "LEFT JOIN chat_mute_settings cms ON cms.chat_id = c.id AND cms.user_id = $1 "
        "LEFT JOIN chat_last_read clr ON clr.chat_id = c.id AND clr.user_id = $1 "
        "ORDER BY (cf.chat_id IS NOT NULL) DESC, c.updated_at DESC",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value chat;
                chat["id"]          = Json::Int64(row["id"].as<long long>());
                chat["type"]        = row["type"].as<std::string>();
                chat["name"]        = row["name"].isNull()        ? Json::Value() : Json::Value(row["name"].as<std::string>());
                chat["title"]       = row["title"].isNull()       ? Json::Value() : Json::Value(row["title"].as<std::string>());
                chat["description"] = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
                chat["public_name"] = row["public_name"].isNull() ? Json::Value() : Json::Value(row["public_name"].as<std::string>());
                chat["updated_at"]  = row["updated_at"].as<std::string>();
                chat["last_message"] = row["last_msg"].isNull() ? Json::Value() : Json::Value(row["last_msg"].as<std::string>());
                chat["last_at"]      = row["last_msg_at"].isNull() ? Json::Value() : Json::Value(row["last_msg_at"].as<std::string>());
                chat["is_favorite"]  = row["is_favorite"].isNull() ? false : row["is_favorite"].as<bool>();
                chat["is_muted"]     = row["is_muted"].isNull()    ? false : row["is_muted"].as<bool>();
                chat["unread_count"] = Json::Int64(row["unread_count"].isNull() ? 0 : row["unread_count"].as<long long>());

                // DM-specific fields
                if (!row["other_user_id"].isNull()) {
                    chat["other_user_id"]      = Json::Int64(row["other_user_id"].as<long long>());
                    chat["other_username"]     = row["other_username"].isNull() ? Json::Value() : Json::Value(row["other_username"].as<std::string>());
                    chat["other_display_name"] = row["other_display_name"].isNull() ? Json::Value() : Json::Value(row["other_display_name"].as<std::string>());

                    std::string avBucket = row["other_avatar_bucket"].isNull() ? "" : row["other_avatar_bucket"].as<std::string>();
                    std::string avKey    = row["other_avatar_key"].isNull()    ? "" : row["other_avatar_key"].as<std::string>();
                    std::string url = avatarUrl(avBucket, avKey);
                    chat["other_avatar_url"] = url.empty() ? Json::Value() : Json::Value(url);
                } else {
                    chat["other_user_id"]      = Json::Value();
                    chat["other_username"]     = Json::Value();
                    chat["other_display_name"] = Json::Value();
                    chat["other_avatar_url"]   = Json::Value();
                }

                arr.append(chat);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listChats: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me);
}

// Helper: verify membership and run callback with (isMember, role)
static void requireMemberChat(long long chatId, long long userId,
                               std::function<void(bool)> cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT 1 FROM chat_members WHERE chat_id = $1 AND user_id = $2",
        [cb](const drogon::orm::Result& r) { cb(!r.empty()); },
        [cb](const drogon::orm::DrogonDbException&) { cb(false); },
        chatId, userId);
}

// POST /chats/{id}/favorite
void ChatsController::addFavorite(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    requireMemberChat(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member", drogon::k403Forbidden));
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO chat_favorites (user_id, chat_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
            [cb](const drogon::orm::Result&) mutable {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "addFavorite: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, me, chatId);
    });
}

// DELETE /chats/{id}/favorite
void ChatsController::removeFavorite(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM chat_favorites WHERE user_id = $1 AND chat_id = $2",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "removeFavorite: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, chatId);
}

// POST /chats/{id}/mute  body: { "muted_until": "<ISO8601>" }  (optional)
void ChatsController::muteChat(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    requireMemberChat(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member", drogon::k403Forbidden));

        auto body = req->getJsonObject();
        std::string mutedUntil = (body && (*body).isMember("muted_until") && !(*body)["muted_until"].isNull())
                                     ? (*body)["muted_until"].asString() : "";

        auto db = drogon::app().getDbClient();
        if (mutedUntil.empty()) {
            db->execSqlAsync(
                "INSERT INTO chat_mute_settings (user_id, chat_id, muted_until) VALUES ($1, $2, NULL) "
                "ON CONFLICT (user_id, chat_id) DO UPDATE SET muted_until = NULL",
                [cb](const drogon::orm::Result&) mutable {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "muteChat: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, me, chatId);
        } else {
            db->execSqlAsync(
                "INSERT INTO chat_mute_settings (user_id, chat_id, muted_until) VALUES ($1, $2, $3::timestamptz) "
                "ON CONFLICT (user_id, chat_id) DO UPDATE SET muted_until = EXCLUDED.muted_until",
                [cb](const drogon::orm::Result&) mutable {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "muteChat timed: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, me, chatId, mutedUntil);
        }
    });
}

// DELETE /chats/{id}/mute
void ChatsController::unmuteChat(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                  long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM chat_mute_settings WHERE user_id = $1 AND chat_id = $2",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "unmuteChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, chatId);
}

// DELETE /chats/{id}/leave
void ChatsController::leaveChat(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                 long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();

    // Check the chat type and user's role first
    db->execSqlAsync(
        "SELECT c.type, cm.role, "
        "    (SELECT COUNT(*) FROM chat_members WHERE chat_id = $1) AS member_count "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
        "WHERE c.id = $1",
        [=, cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("Not a member of this chat", drogon::k404NotFound));

            std::string role = r[0]["role"].as<std::string>();
            long long memberCount = r[0]["member_count"].as<long long>();

            // Prevent sole owner from leaving (would orphan the chat)
            if (role == "owner" && memberCount == 1) {
                // Single member who is owner — allow leave (effectively deletes)
            } else if (role == "owner") {
                return cb(jsonErr(
                    "Transfer ownership before leaving, or delete the chat",
                    drogon::k400BadRequest));
            }

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "DELETE FROM chat_members WHERE chat_id = $1 AND user_id = $2",
                [cb](const drogon::orm::Result&) mutable {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    cb(resp);
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "leaveChat delete: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId, me);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "leaveChat check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// GET /chats/{id}
void ChatsController::getChat(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                               long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.title, c.description, c.public_name, c.owner_id, c.created_at "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id "
        "WHERE c.id = $1 AND cm.user_id = $2",
        [cb, chatId, me](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));
            const auto row = r[0];
            Json::Value chat;
            chat["id"]          = Json::Int64(row["id"].as<long long>());
            chat["type"]        = row["type"].as<std::string>();
            chat["name"]        = row["name"].isNull()        ? Json::Value() : Json::Value(row["name"].as<std::string>());
            chat["title"]       = row["title"].isNull()       ? Json::Value() : Json::Value(row["title"].as<std::string>());
            chat["description"] = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
            chat["public_name"] = row["public_name"].isNull() ? Json::Value() : Json::Value(row["public_name"].as<std::string>());
            chat["owner_id"]    = Json::Int64(row["owner_id"].as<long long>());
            chat["created_at"]  = row["created_at"].as<std::string>();

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT u.id, u.username, u.display_name, cm.role "
                "FROM chat_members cm JOIN users u ON u.id = cm.user_id "
                "WHERE cm.chat_id = $1",
                [cb, chat = std::move(chat), me](const drogon::orm::Result& mr) mutable {
                    Json::Value members(Json::arrayValue);
                    for (auto& m : mr) {
                        Json::Value mem;
                        mem["id"]           = Json::Int64(m["id"].as<long long>());
                        mem["username"]     = m["username"].as<std::string>();
                        mem["display_name"] = m["display_name"].isNull() ? Json::Value() : Json::Value(m["display_name"].as<std::string>());
                        mem["role"]         = m["role"].as<std::string>();
                        members.append(mem);
                        if (m["id"].as<long long>() == me)
                            chat["my_role"] = m["role"].as<std::string>();
                    }
                    chat["members"] = members;
                    if (chat["my_role"].isNull()) chat["my_role"] = "member";
                    cb(drogon::HttpResponse::newHttpJsonResponse(chat));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "getChat members: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// POST /chats/{id}/read — mark all messages in chat as read
void ChatsController::markRead(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO chat_last_read (user_id, chat_id, last_read_msg_id, read_at) "
        "SELECT $1, $2, COALESCE(MAX(m.id), 0), NOW() FROM messages m WHERE m.chat_id = $2 "
        "ON CONFLICT (user_id, chat_id) DO UPDATE SET "
        "  last_read_msg_id = GREATEST(chat_last_read.last_read_msg_id, EXCLUDED.last_read_msg_id), "
        "  read_at = NOW()",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "markRead: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, chatId);
}

// GET /chats/by-name/{publicName} (public endpoint, no AuthFilter)
void ChatsController::getChatByPublicName(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                           const std::string& publicName) {
    if (publicName.empty())
        return cb(jsonErr("public_name is required", drogon::k400BadRequest));

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.title, c.description, c.public_name, c.created_at, "
        "       (SELECT COUNT(*) FROM chat_members cm WHERE cm.chat_id = c.id) AS member_count "
        "FROM chats c "
        "WHERE c.public_name = $1",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("Chat not found", drogon::k404NotFound));
            const auto row = r[0];
            Json::Value chat;
            chat["id"]           = Json::Int64(row["id"].as<long long>());
            chat["type"]         = row["type"].as<std::string>();
            chat["name"]         = row["name"].isNull()        ? Json::Value() : Json::Value(row["name"].as<std::string>());
            chat["title"]        = row["title"].isNull()       ? Json::Value() : Json::Value(row["title"].as<std::string>());
            chat["description"]  = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
            chat["public_name"]  = row["public_name"].isNull() ? Json::Value() : Json::Value(row["public_name"].as<std::string>());
            chat["created_at"]   = row["created_at"].as<std::string>();
            chat["member_count"] = Json::Int64(row["member_count"].as<long long>());
            cb(drogon::HttpResponse::newHttpJsonResponse(chat));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getChatByPublicName: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, publicName);
}

// POST /chats/{id}/pin
void ChatsController::pinChat(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                               long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    requireMemberChat(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member", drogon::k403Forbidden));
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO pinned_chats (user_id, chat_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
            [cb](const drogon::orm::Result&) mutable {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "pinChat: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, me, chatId);
    });
}

// DELETE /chats/{id}/pin
void ChatsController::unpinChat(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                 long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM pinned_chats WHERE user_id = $1 AND chat_id = $2",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "unpinChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, chatId);
}

// POST /chats/{id}/archive
void ChatsController::archiveChat(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    requireMemberChat(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member", drogon::k403Forbidden));
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO archived_chats (user_id, chat_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
            [cb](const drogon::orm::Result&) mutable {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                cb(resp);
            },
            [cb](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "archiveChat: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }, me, chatId);
    });
}

// DELETE /chats/{id}/archive
void ChatsController::unarchiveChat(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                     long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM archived_chats WHERE user_id = $1 AND chat_id = $2",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "unarchiveChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, chatId);
}
