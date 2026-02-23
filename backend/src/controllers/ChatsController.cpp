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

    // For channels, set name=title for backward compatibility
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

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO chats (type, name, title, description, public_name, owner_id) "
        "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
        [cb, members, me, type, title](const drogon::orm::Result& r) mutable {
            long long chatId = r[0]["id"].as<long long>();
            auto db2 = drogon::app().getDbClient();

            // Insert all members
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

// GET /chats
void ChatsController::listChats(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT c.id, c.type, c.name, c.title, c.description, c.public_name, c.updated_at, "
        "       (SELECT content FROM messages m WHERE m.chat_id = c.id "
        "        ORDER BY m.created_at DESC LIMIT 1) AS last_msg "
        "FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id "
        "WHERE cm.user_id = $1 "
        "ORDER BY c.updated_at DESC",
        [cb](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value chat;
                chat["id"]         = Json::Int64(row["id"].as<long long>());
                chat["type"]       = row["type"].as<std::string>();
                chat["name"]       = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
                chat["title"]      = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
                chat["description"] = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
                chat["public_name"] = row["public_name"].isNull() ? Json::Value() : Json::Value(row["public_name"].as<std::string>());
                chat["updated_at"] = row["updated_at"].as<std::string>();
                chat["last_message"] = row["last_msg"].isNull() ? Json::Value() : Json::Value(row["last_msg"].as<std::string>());
                arr.append(chat);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
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
            chat["name"]        = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
            chat["title"]       = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
            chat["description"] = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
            chat["public_name"] = row["public_name"].isNull() ? Json::Value() : Json::Value(row["public_name"].as<std::string>());
            chat["owner_id"]    = Json::Int64(row["owner_id"].as<long long>());
            chat["created_at"]  = row["created_at"].as<std::string>();

            // Fetch members
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
                        mem["display_name"] = m["display_name"].as<std::string>();
                        mem["role"]         = m["role"].as<std::string>();
                        members.append(mem);
                        // Set my_role for the requesting user
                        if (m["id"].as<long long>() == me) {
                            chat["my_role"] = m["role"].as<std::string>();
                        }
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
            chat["name"]         = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
            chat["title"]        = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
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
