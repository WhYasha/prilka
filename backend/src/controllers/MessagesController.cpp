#include "MessagesController.h"
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

// Check membership helper: returns false via callback if user is not in chat
static void requireMember(long long chatId, long long userId,
                           std::function<void(bool)> cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT 1 FROM chat_members WHERE chat_id = $1 AND user_id = $2",
        [cb](const drogon::orm::Result& r) { cb(!r.empty()); },
        [cb](const drogon::orm::DrogonDbException&) { cb(false); },
        chatId, userId);
}

// POST /chats/{id}/messages
void MessagesController::sendMessage(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string content  = (*body)["content"].asString();
    std::string msgType  = (*body).get("type", "text").asString();
    long long   fileId   = (*body).get("file_id", Json::Value(0)).asInt64();

    if (content.empty() && fileId == 0)
        return cb(jsonErr("content or file_id required", drogon::k400BadRequest));

    requireMember(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Check channel write permission
        auto db0 = drogon::app().getDbClient();
        db0->execSqlAsync(
            "SELECT c.type, cm.role FROM chats c "
            "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
            "WHERE c.id = $1",
            [=, cb = std::move(cb)](const drogon::orm::Result& pr) mutable {
                if (!pr.empty()) {
                    std::string chatType = pr[0]["type"].as<std::string>();
                    std::string role = pr[0]["role"].as<std::string>();
                    if (chatType == "channel" && role != "owner" && role != "admin") {
                        return cb(jsonErr("Only admins can post in channels", drogon::k403Forbidden));
                    }
                }

        auto db = drogon::app().getDbClient();
        std::string sql = fileId > 0
            ? "INSERT INTO messages (chat_id, sender_id, content, message_type, file_id) "
              "VALUES ($1, $2, $3, $4, $5) RETURNING id, created_at"
            : "INSERT INTO messages (chat_id, sender_id, content, message_type) "
              "VALUES ($1, $2, $3, $4) RETURNING id, created_at";

        auto insert = [=, cb](const drogon::orm::Result& r) mutable {
            long long msgId = r[0]["id"].as<long long>();
            std::string createdAt = r[0]["created_at"].as<std::string>();

            // Update chat.updated_at
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE chats SET updated_at = NOW() WHERE id = $1",
                [](const drogon::orm::Result&) {},
                [](const drogon::orm::DrogonDbException& e) {
                    LOG_WARN << "chat updated_at: " << e.base().what();
                }, chatId);

            // Build WS payload and fan-out
            Json::Value wsMsg;
            wsMsg["type"]       = "message";
            wsMsg["id"]         = Json::Int64(msgId);
            wsMsg["chat_id"]    = Json::Int64(chatId);
            wsMsg["sender_id"]  = Json::Int64(me);
            wsMsg["content"]    = content;
            wsMsg["message_type"] = msgType;
            wsMsg["created_at"] = createdAt;
            WsDispatch::publishMessage(chatId, wsMsg);

            Json::Value resp;
            resp["id"]         = Json::Int64(msgId);
            resp["chat_id"]    = Json::Int64(chatId);
            resp["sender_id"]  = Json::Int64(me);
            resp["content"]    = content;
            resp["type"]       = msgType;
            resp["created_at"] = createdAt;
            auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
            httpResp->setStatusCode(drogon::k201Created);
            cb(httpResp);
        };
        auto onErr = [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "sendMessage: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        };

        if (fileId > 0)
            db->execSqlAsync(sql, std::move(insert), std::move(onErr),
                             chatId, me, content, msgType, fileId);
        else
            db->execSqlAsync(sql, std::move(insert), std::move(onErr),
                             chatId, me, content, msgType);
            },
            [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                LOG_ERROR << "channel permission check: " << e.base().what();
                cb(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            chatId, me);
    });
}

// GET /chats/{id}/messages?limit=50&before=<message_id>
void MessagesController::listMessages(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                       long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");

    requireMember(chatId, me, [=, cb = std::move(cb)](bool isMember) mutable {
        if (!isMember) return cb(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Use long long — Drogon's PG driver serialises int as 4 bytes but the
        // wire-protocol parameter slot expects 8 bytes, causing "insufficient data".
        long long limit = std::min(std::max(std::stoi(req->getParameter("limit").empty()
                                                      ? "50"
                                                      : req->getParameter("limit")), 1), 100);
        std::string beforeStr = req->getParameter("before");

        auto db = drogon::app().getDbClient();
        auto handleRows = [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value msg;
                msg["id"]           = Json::Int64(row["id"].as<long long>());
                msg["chat_id"]      = Json::Int64(row["chat_id"].as<long long>());
                msg["sender_id"]    = Json::Int64(row["sender_id"].as<long long>());
                msg["content"]      = row["content"].isNull() ? Json::Value() : Json::Value(row["content"].as<std::string>());
                msg["message_type"] = row["message_type"].as<std::string>();
                msg["created_at"]   = row["created_at"].as<std::string>();
                arr.append(msg);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        };
        auto onErr = [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listMessages: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        };

        if (!beforeStr.empty()) {
            long long before = std::stoll(beforeStr);
            db->execSqlAsync(
                "SELECT id, chat_id, sender_id, content, message_type, created_at "
                "FROM messages WHERE chat_id = $1 AND id < $2 "
                "ORDER BY created_at DESC LIMIT $3",
                std::move(handleRows), std::move(onErr), chatId, before, limit);
        } else {
            db->execSqlAsync(
                "SELECT id, chat_id, sender_id, content, message_type, created_at "
                "FROM messages WHERE chat_id = $1 "
                "ORDER BY created_at DESC LIMIT $2",
                std::move(handleRows), std::move(onErr), chatId, limit);
        }
    });
}
