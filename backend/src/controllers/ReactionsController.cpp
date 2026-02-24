#include "ReactionsController.h"
#include "../ws/WsHandler.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>
#include <vector>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
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

// POST /chats/{chatId}/messages/{messageId}/reactions
void ReactionsController::toggleReaction(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
    long long chatId, long long messageId)
{
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string emoji = (*body).get("emoji", "").asString();
    if (emoji.empty() || emoji.size() > 32)
        return cb(jsonErr("emoji required (max 32 chars)", drogon::k400BadRequest));

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        // Verify message belongs to this chat
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT 1 FROM messages WHERE id = $1 AND chat_id = $2",
            [=](const drogon::orm::Result& mr) {
                if (mr.empty())
                    return (*cbPtr)(jsonErr("Message not found in this chat", drogon::k404NotFound));

                // Try DELETE first
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "DELETE FROM message_reactions WHERE message_id = $1 AND user_id = $2 AND emoji = $3",
                    [=](const drogon::orm::Result& dr) {
                        if (dr.affectedRows() > 0) {
                            // Removed
                            Json::Value wsPayload;
                            wsPayload["type"]       = "reaction";
                            wsPayload["chat_id"]    = Json::Int64(chatId);
                            wsPayload["message_id"] = Json::Int64(messageId);
                            wsPayload["user_id"]    = Json::Int64(me);
                            wsPayload["emoji"]      = emoji;
                            wsPayload["action"]     = "removed";
                            WsDispatch::publishMessage(chatId, wsPayload);

                            Json::Value resp;
                            resp["message_id"] = Json::Int64(messageId);
                            resp["emoji"]      = emoji;
                            resp["action"]     = "removed";
                            (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
                        } else {
                            // INSERT
                            auto db3 = drogon::app().getDbClient();
                            db3->execSqlAsync(
                                "INSERT INTO message_reactions (message_id, user_id, emoji) "
                                "VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                                [=](const drogon::orm::Result&) {
                                    Json::Value wsPayload;
                                    wsPayload["type"]       = "reaction";
                                    wsPayload["chat_id"]    = Json::Int64(chatId);
                                    wsPayload["message_id"] = Json::Int64(messageId);
                                    wsPayload["user_id"]    = Json::Int64(me);
                                    wsPayload["emoji"]      = emoji;
                                    wsPayload["action"]     = "added";
                                    WsDispatch::publishMessage(chatId, wsPayload);

                                    Json::Value resp;
                                    resp["message_id"] = Json::Int64(messageId);
                                    resp["emoji"]      = emoji;
                                    resp["action"]     = "added";
                                    (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(resp));
                                },
                                [cbPtr](const drogon::orm::DrogonDbException& e) {
                                    LOG_ERROR << "reaction insert: " << e.base().what();
                                    (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                                },
                                messageId, me, emoji);
                        }
                    },
                    [cbPtr](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "reaction delete: " << e.base().what();
                        (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
                    },
                    messageId, me, emoji);
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "reaction msg check: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            messageId, chatId);
    });
}

// GET /chats/{chatId}/reactions?message_ids=1,2,3
void ReactionsController::getReactions(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
    long long chatId)
{
    long long me = req->getAttributes()->get<long long>("user_id");

    std::string idsParam = req->getParameter("message_ids");
    if (idsParam.empty())
        return cb(jsonErr("message_ids required", drogon::k400BadRequest));

    // Parse comma-separated IDs
    std::vector<long long> ids;
    std::istringstream ss(idsParam);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            long long id = std::stoll(token);
            if (id > 0) ids.push_back(id);
        } catch (...) {}
    }
    if (ids.empty() || ids.size() > 200)
        return cb(jsonErr("message_ids: 1-200 valid IDs required", drogon::k400BadRequest));

    // Build PostgreSQL array literal
    std::string pgArray = "{";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) pgArray += ",";
        pgArray += std::to_string(ids[i]);
    }
    pgArray += "}";

    using CbT = std::function<void(const drogon::HttpResponsePtr&)>;
    auto cbPtr = std::make_shared<CbT>(std::move(cb));

    requireMember(chatId, me, [=](bool isMember) {
        if (!isMember) return (*cbPtr)(jsonErr("Not a member of this chat", drogon::k403Forbidden));

        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT message_id, emoji, COUNT(*) AS count, "
            "BOOL_OR(user_id = $2) AS me "
            "FROM message_reactions WHERE message_id = ANY($1::bigint[]) "
            "GROUP BY message_id, emoji ORDER BY message_id, MIN(created_at)",
            [=](const drogon::orm::Result& r) {
                Json::Value result(Json::objectValue);
                for (const auto& row : r) {
                    std::string msgIdStr = std::to_string(row["message_id"].as<long long>());
                    if (!result.isMember(msgIdStr))
                        result[msgIdStr] = Json::Value(Json::arrayValue);

                    Json::Value entry;
                    entry["emoji"] = row["emoji"].as<std::string>();
                    entry["count"] = row["count"].as<int>();
                    entry["me"]    = row["me"].as<bool>();
                    result[msgIdStr].append(entry);
                }
                (*cbPtr)(drogon::HttpResponse::newHttpJsonResponse(result));
            },
            [cbPtr](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "getReactions: " << e.base().what();
                (*cbPtr)(jsonErr("Internal error", drogon::k500InternalServerError));
            },
            pgArray, me);
    });
}
