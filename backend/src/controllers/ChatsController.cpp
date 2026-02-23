#include "ChatsController.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

// POST /chats
// Body: { "type": "direct"|"group", "name": "...", "member_ids": [2, 3] }
void ChatsController::createChat(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string type = (*body).get("type", "direct").asString();
    if (type != "direct" && type != "group")
        return cb(jsonErr("type must be 'direct' or 'group'", drogon::k400BadRequest));

    std::string name = (*body).get("name", "").asString();
    auto memberIds = (*body)["member_ids"];

    std::vector<long long> members;
    members.push_back(me);
    for (auto& id : memberIds) {
        long long mid = id.asInt64();
        if (mid != me) members.push_back(mid);
    }

    if (type == "direct" && members.size() != 2)
        return cb(jsonErr("direct chat requires exactly one other member_id", drogon::k400BadRequest));

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO chats (type, name, owner_id) VALUES ($1, $2, $3) RETURNING id",
        [cb = std::move(cb), members, me](const drogon::orm::Result& r) mutable {
            long long chatId = r[0]["id"].as<long long>();
            auto db2 = drogon::app().getDbClient();

            // Insert all members
            // For MVP: sequential inserts (TODO: use COPY or multi-row INSERT)
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
            resp["id"]       = Json::Int64(chatId);
            resp["type"]     = r.empty() ? "group" : "created";
            auto respObj = drogon::HttpResponse::newHttpJsonResponse(resp);
            respObj->setStatusCode(drogon::k201Created);
            cb(respObj);
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "createChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, type, name, me);
}

// GET /chats
void ChatsController::listChats(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.updated_at, "
        "       (SELECT content FROM messages m WHERE m.chat_id = c.id "
        "        ORDER BY m.created_at DESC LIMIT 1) AS last_msg "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id "
        "WHERE cm.user_id = $1 "
        "ORDER BY c.updated_at DESC",
        [cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value chat;
                chat["id"]         = Json::Int64(row["id"].as<long long>());
                chat["type"]       = row["type"].as<std::string>();
                chat["name"]       = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
                chat["updated_at"] = row["updated_at"].as<std::string>();
                chat["last_message"] = row["last_msg"].isNull() ? Json::Value() : Json::Value(row["last_msg"].as<std::string>());
                arr.append(chat);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listChats: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me);
}

// GET /chats/{id}
void ChatsController::getChat(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                               long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.owner_id, c.created_at "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id "
        "WHERE c.id = $1 AND cm.user_id = $2",
        [cb = std::move(cb), chatId](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));
            const auto row = r[0];
            Json::Value chat;
            chat["id"]         = Json::Int64(row["id"].as<long long>());
            chat["type"]       = row["type"].as<std::string>();
            chat["name"]       = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
            chat["owner_id"]   = Json::Int64(row["owner_id"].as<long long>());
            chat["created_at"] = row["created_at"].as<std::string>();

            // Fetch members
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT u.id, u.username, u.display_name, cm.role "
                "FROM chat_members cm JOIN users u ON u.id = cm.user_id "
                "WHERE cm.chat_id = $1",
                [cb = std::move(cb), chat = std::move(chat)](const drogon::orm::Result& mr) mutable {
                    Json::Value members(Json::arrayValue);
                    for (auto& m : mr) {
                        Json::Value mem;
                        mem["id"]           = Json::Int64(m["id"].as<long long>());
                        mem["username"]     = m["username"].as<std::string>();
                        mem["display_name"] = m["display_name"].as<std::string>();
                        mem["role"]         = m["role"].as<std::string>();
                        members.append(mem);
                    }
                    chat["members"] = members;
                    cb(drogon::HttpResponse::newHttpJsonResponse(chat));
                },
                [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "getChat members: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId);
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getChat: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}
