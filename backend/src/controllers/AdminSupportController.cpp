#include "AdminSupportController.h"
#include "../ws/WsHandler.h"
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}

static drogon::HttpResponsePtr errResp(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    return jsonResp(b, code);
}

// ── GET /admin-api/support/users ────────────────────────────────────────────

void AdminSupportController::listUsers(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT id, username, display_name FROM users "
        "WHERE is_active = TRUE AND username != 'bh_support' "
        "ORDER BY username",
        [cbSh](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value u;
                u["id"]           = Json::Int64(row["id"].as<long long>());
                u["username"]     = row["username"].as<std::string>();
                u["display_name"] = row["display_name"].isNull()
                    ? Json::Value(row["username"].as<std::string>())
                    : Json::Value(row["display_name"].as<std::string>());
                arr.append(u);
            }
            Json::Value resp;
            resp["users"] = arr;
            (*cbSh)(jsonResp(resp, drogon::k200OK));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin support users: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        });
}

// ── GET /admin-api/support/chats ────────────────────────────────────────────

void AdminSupportController::listChats(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT c.id, c.type::TEXT AS type, c.name, c.title, "
        "(SELECT COUNT(*) FROM chat_members WHERE chat_id = c.id) AS member_count "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id "
        "JOIN users u ON u.id = cm.user_id AND u.username = 'bh_support' "
        "ORDER BY c.updated_at DESC",
        [cbSh](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value c;
                c["id"]           = Json::Int64(row["id"].as<long long>());
                c["type"]         = row["type"].as<std::string>();
                c["name"]         = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
                c["title"]        = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
                c["member_count"] = Json::Int64(row["member_count"].as<long long>());
                arr.append(c);
            }
            Json::Value resp;
            resp["chats"] = arr;
            (*cbSh)(jsonResp(resp, drogon::k200OK));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin support chats: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        });
}

// ── GET /admin-api/support/messages ─────────────────────────────────────────

void AdminSupportController::listMessages(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT m.id, m.chat_id, m.content, m.created_at, "
        "c.name AS chat_name, c.type::TEXT AS chat_type "
        "FROM messages m "
        "JOIN users u ON u.id = m.sender_id AND u.username = 'bh_support' "
        "JOIN chats c ON c.id = m.chat_id "
        "ORDER BY m.created_at DESC LIMIT 50",
        [cbSh](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value m;
                m["id"]        = Json::Int64(row["id"].as<long long>());
                m["chat_id"]   = Json::Int64(row["chat_id"].as<long long>());
                m["content"]   = row["content"].isNull() ? Json::Value() : Json::Value(row["content"].as<std::string>());
                m["created_at"]= row["created_at"].as<std::string>();
                m["chat_name"] = row["chat_name"].isNull() ? Json::Value() : Json::Value(row["chat_name"].as<std::string>());
                m["chat_type"] = row["chat_type"].as<std::string>();
                arr.append(m);
            }
            Json::Value resp;
            resp["messages"] = arr;
            (*cbSh)(jsonResp(resp, drogon::k200OK));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin support messages: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        });
}

// ── POST /admin-api/support/send ────────────────────────────────────────────

void AdminSupportController::sendMessage(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    auto body = req->getJsonObject();
    if (!body) return cb(errResp("Invalid JSON", drogon::k400BadRequest));

    std::string content = (*body)["content"].asString();
    if (content.empty()) return cb(errResp("content required", drogon::k400BadRequest));

    long long targetUserId = 0;
    long long chatId = 0;
    if (body->isMember("target_user_id") && !(*body)["target_user_id"].isNull())
        targetUserId = (*body)["target_user_id"].asInt64();
    if (body->isMember("chat_id") && !(*body)["chat_id"].isNull())
        chatId = (*body)["chat_id"].asInt64();

    if (targetUserId == 0 && chatId == 0)
        return cb(errResp("target_user_id or chat_id required", drogon::k400BadRequest));

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    std::string markedContent = "[SUPPORT] " + content;

    // Step 1: Get bh_support user ID
    db->execSqlAsync(
        "SELECT id FROM users WHERE username = 'bh_support'",
        [cbSh, db, targetUserId, chatId, markedContent](const drogon::orm::Result& r) {
            if (r.empty()) {
                (*cbSh)(errResp("bh_support user not found", drogon::k500InternalServerError));
                return;
            }
            long long supportId = r[0]["id"].as<long long>();

            if (targetUserId > 0) {
                // Find or create DM between bh_support and target
                db->execSqlAsync(
                    "SELECT c.id FROM chats c "
                    "JOIN chat_members cm1 ON cm1.chat_id = c.id AND cm1.user_id = $1 "
                    "JOIN chat_members cm2 ON cm2.chat_id = c.id AND cm2.user_id = $2 "
                    "WHERE c.type = 'direct'",
                    [cbSh, db, supportId, targetUserId, markedContent](const drogon::orm::Result& r2) {
                        if (!r2.empty()) {
                            // DM exists — send message
                            long long actualChatId = r2[0]["id"].as<long long>();
                            db->execSqlAsync(
                                "INSERT INTO messages (chat_id, sender_id, content, message_type) "
                                "VALUES ($1, $2, $3, 'text') RETURNING id, created_at",
                                [cbSh, db, actualChatId, supportId, markedContent](const drogon::orm::Result& r3) {
                                    long long msgId = r3[0]["id"].as<long long>();
                                    std::string createdAt = r3[0]["created_at"].as<std::string>();
                                    db->execSqlAsync(
                                        "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                                        [](const drogon::orm::Result&) {},
                                        [](const drogon::orm::DrogonDbException&) {},
                                        actualChatId);

                                    // Publish to Redis
                                    Json::Value wsMsg;
                                    wsMsg["type"]         = "message";
                                    wsMsg["id"]           = Json::Int64(msgId);
                                    wsMsg["chat_id"]      = Json::Int64(actualChatId);
                                    wsMsg["sender_id"]    = Json::Int64(supportId);
                                    wsMsg["content"]      = markedContent;
                                    wsMsg["message_type"] = "text";
                                    wsMsg["created_at"]   = createdAt;
                                    WsDispatch::publishMessage(actualChatId, wsMsg);

                                    Json::Value resp;
                                    resp["id"]         = Json::Int64(msgId);
                                    resp["chat_id"]    = Json::Int64(actualChatId);
                                    resp["created_at"] = createdAt;
                                    (*cbSh)(jsonResp(resp, drogon::k201Created));
                                },
                                [cbSh](const drogon::orm::DrogonDbException& e) {
                                    LOG_ERROR << "support send msg: " << e.base().what();
                                    (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                                },
                                actualChatId, supportId, markedContent);
                        } else {
                            // Create new DM
                            db->execSqlAsync(
                                "INSERT INTO chats (type, owner_id) VALUES ('direct', $1) RETURNING id",
                                [cbSh, db, supportId, targetUserId, markedContent](const drogon::orm::Result& r3) {
                                    long long newChatId = r3[0]["id"].as<long long>();
                                    // Insert both members
                                    db->execSqlAsync(
                                        "INSERT INTO chat_members (chat_id, user_id, role) "
                                        "VALUES ($1, $2, 'owner'), ($1, $3, 'member')",
                                        [cbSh, db, newChatId, supportId, markedContent](const drogon::orm::Result&) {
                                            // Now send the message
                                            db->execSqlAsync(
                                                "INSERT INTO messages (chat_id, sender_id, content, message_type) "
                                                "VALUES ($1, $2, $3, 'text') RETURNING id, created_at",
                                                [cbSh, db, newChatId, supportId, markedContent](const drogon::orm::Result& r4) {
                                                    long long msgId = r4[0]["id"].as<long long>();
                                                    std::string createdAt = r4[0]["created_at"].as<std::string>();
                                                    db->execSqlAsync(
                                                        "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                                                        [](const drogon::orm::Result&) {},
                                                        [](const drogon::orm::DrogonDbException&) {},
                                                        newChatId);

                                                    Json::Value wsMsg;
                                                    wsMsg["type"]         = "message";
                                                    wsMsg["id"]           = Json::Int64(msgId);
                                                    wsMsg["chat_id"]      = Json::Int64(newChatId);
                                                    wsMsg["sender_id"]    = Json::Int64(supportId);
                                                    wsMsg["content"]      = markedContent;
                                                    wsMsg["message_type"] = "text";
                                                    wsMsg["created_at"]   = createdAt;
                                                    WsDispatch::publishMessage(newChatId, wsMsg);

                                                    Json::Value resp;
                                                    resp["id"]         = Json::Int64(msgId);
                                                    resp["chat_id"]    = Json::Int64(newChatId);
                                                    resp["created_at"] = createdAt;
                                                    (*cbSh)(jsonResp(resp, drogon::k201Created));
                                                },
                                                [cbSh](const drogon::orm::DrogonDbException& e) {
                                                    LOG_ERROR << "support send msg: " << e.base().what();
                                                    (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                                                },
                                                newChatId, supportId, markedContent);
                                        },
                                        [cbSh](const drogon::orm::DrogonDbException& e) {
                                            LOG_ERROR << "support add members: " << e.base().what();
                                            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                                        },
                                        newChatId, supportId, targetUserId);
                                },
                                [cbSh](const drogon::orm::DrogonDbException& e) {
                                    LOG_ERROR << "support create chat: " << e.base().what();
                                    (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                                },
                                supportId);
                        }
                    },
                    [cbSh](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "support find DM: " << e.base().what();
                        (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                    },
                    supportId, targetUserId);
            } else {
                // chatId provided — ensure bh_support is a member, then send
                db->execSqlAsync(
                    "INSERT INTO chat_members (chat_id, user_id, role) "
                    "VALUES ($1, $2, 'admin') ON CONFLICT DO NOTHING",
                    [cbSh, db, chatId, supportId, markedContent](const drogon::orm::Result&) {
                        db->execSqlAsync(
                            "INSERT INTO messages (chat_id, sender_id, content, message_type) "
                            "VALUES ($1, $2, $3, 'text') RETURNING id, created_at",
                            [cbSh, db, chatId, supportId, markedContent](const drogon::orm::Result& r3) {
                                long long msgId = r3[0]["id"].as<long long>();
                                std::string createdAt = r3[0]["created_at"].as<std::string>();
                                db->execSqlAsync(
                                    "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                                    [](const drogon::orm::Result&) {},
                                    [](const drogon::orm::DrogonDbException&) {},
                                    chatId);

                                Json::Value wsMsg;
                                wsMsg["type"]         = "message";
                                wsMsg["id"]           = Json::Int64(msgId);
                                wsMsg["chat_id"]      = Json::Int64(chatId);
                                wsMsg["sender_id"]    = Json::Int64(supportId);
                                wsMsg["content"]      = markedContent;
                                wsMsg["message_type"] = "text";
                                wsMsg["created_at"]   = createdAt;
                                WsDispatch::publishMessage(chatId, wsMsg);

                                Json::Value resp;
                                resp["id"]         = Json::Int64(msgId);
                                resp["chat_id"]    = Json::Int64(chatId);
                                resp["created_at"] = createdAt;
                                (*cbSh)(jsonResp(resp, drogon::k201Created));
                            },
                            [cbSh](const drogon::orm::DrogonDbException& e) {
                                LOG_ERROR << "support send msg: " << e.base().what();
                                (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                            },
                            chatId, supportId, markedContent);
                    },
                    [cbSh](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "support ensure member: " << e.base().what();
                        (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                    },
                    chatId, supportId);
            }
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "support find bh_support: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        });
}
