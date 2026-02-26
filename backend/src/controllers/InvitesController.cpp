#include "InvitesController.h"
#include "../ws/WsHandler.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

// POST /chats/{id}/invites
void InvitesController::createInvite(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      long long chatId) {
    long long me;
    try {
        me = req->getAttributes()->get<long long>("user_id");
    } catch (...) {
        return cb(jsonErr("Unauthorized", drogon::k401Unauthorized));
    }

    drogon::orm::DbClientPtr db;
    try {
        db = drogon::app().getDbClient();
    } catch (const std::exception& e) {
        LOG_ERROR << "createInvite getDbClient: " << e.what();
        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
    }

    // Check chat type and user role
    db->execSqlAsync(
        "SELECT c.type, cm.role FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
        "WHERE c.id = $1",
        [cb, chatId, me](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));

            std::string chatType = r[0]["type"].as<std::string>();
            std::string role = r[0]["role"].as<std::string>();

            if (chatType == "direct")
                return cb(jsonErr("Cannot create invites for direct chats", drogon::k400BadRequest));

            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can create invites", drogon::k403Forbidden));

            // Insert invite
            drogon::orm::DbClientPtr db2;
            try { db2 = drogon::app().getDbClient(); }
            catch (const std::exception& ex) {
                LOG_ERROR << "createInvite inner getDbClient: " << ex.what();
                return cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }
            db2->execSqlAsync(
                "INSERT INTO invites (chat_id, created_by) VALUES ($1, $2) "
                "RETURNING token, chat_id, created_by, created_at",
                [cb](const drogon::orm::Result& ir) mutable {
                    const auto& row = ir[0];
                    std::string token = row["token"].as<std::string>();

                    Json::Value resp;
                    resp["token"]      = token;
                    resp["chat_id"]    = Json::Int64(row["chat_id"].as<long long>());
                    resp["created_by"] = Json::Int64(row["created_by"].as<long long>());
                    resp["created_at"] = row["created_at"].as<std::string>();
                    resp["link"]       = "/join/" + token;

                    auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
                    httpResp->setStatusCode(drogon::k201Created);
                    cb(httpResp);
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "createInvite: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId, me);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "createInvite check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// GET /chats/{id}/invites
void InvitesController::listInvites(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                     long long chatId) {
    long long me;
    try {
        me = req->getAttributes()->get<long long>("user_id");
    } catch (...) {
        return cb(jsonErr("Unauthorized", drogon::k401Unauthorized));
    }

    drogon::orm::DbClientPtr db;
    try {
        db = drogon::app().getDbClient();
    } catch (const std::exception& e) {
        LOG_ERROR << "listInvites getDbClient: " << e.what();
        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
    }

    // Check user role
    db->execSqlAsync(
        "SELECT cm.role FROM chat_members cm WHERE cm.chat_id = $1 AND cm.user_id = $2",
        [cb, chatId](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));

            std::string role = r[0]["role"].as<std::string>();
            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can list invites", drogon::k403Forbidden));

            drogon::orm::DbClientPtr db2;
            try { db2 = drogon::app().getDbClient(); }
            catch (const std::exception& ex) {
                LOG_ERROR << "listInvites inner getDbClient: " << ex.what();
                return cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }
            db2->execSqlAsync(
                "SELECT token, chat_id, created_by, created_at FROM invites "
                "WHERE chat_id = $1 AND revoked_at IS NULL "
                "ORDER BY created_at DESC",
                [cb](const drogon::orm::Result& ir) mutable {
                    Json::Value arr(Json::arrayValue);
                    for (auto& row : ir) {
                        Json::Value inv;
                        std::string token = row["token"].as<std::string>();
                        inv["token"]      = token;
                        inv["chat_id"]    = Json::Int64(row["chat_id"].as<long long>());
                        inv["created_by"] = Json::Int64(row["created_by"].as<long long>());
                        inv["created_at"] = row["created_at"].as<std::string>();
                        inv["link"]       = "/join/" + token;
                        arr.append(inv);
                    }
                    cb(drogon::HttpResponse::newHttpJsonResponse(arr));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "listInvites: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listInvites check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// DELETE /invites/{token}
void InvitesController::revokeInvite(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      const std::string& token) {
    long long me;
    try {
        me = req->getAttributes()->get<long long>("user_id");
    } catch (...) {
        return cb(jsonErr("Unauthorized", drogon::k401Unauthorized));
    }

    drogon::orm::DbClientPtr db;
    try {
        db = drogon::app().getDbClient();
    } catch (const std::exception& e) {
        LOG_ERROR << "revokeInvite getDbClient: " << e.what();
        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
    }

    // Find invite and check ownership
    db->execSqlAsync(
        "SELECT i.chat_id, cm.role FROM invites i "
        "JOIN chat_members cm ON cm.chat_id = i.chat_id AND cm.user_id = $2 "
        "WHERE i.token = $1 AND i.revoked_at IS NULL",
        [cb, token](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Invite not found or already revoked", drogon::k404NotFound));

            std::string role = r[0]["role"].as<std::string>();
            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can revoke invites", drogon::k403Forbidden));

            drogon::orm::DbClientPtr db2;
            try { db2 = drogon::app().getDbClient(); }
            catch (const std::exception& ex) {
                LOG_ERROR << "revokeInvite inner getDbClient: " << ex.what();
                return cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }
            db2->execSqlAsync(
                "UPDATE invites SET revoked_at = NOW() WHERE token = $1",
                [cb](const drogon::orm::Result&) mutable {
                    Json::Value resp;
                    resp["revoked"] = true;
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "revokeInvite: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, token);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "revokeInvite check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, token, me);
}

// GET /invites/{token}/preview (public, no AuthFilter)
void InvitesController::previewInvite(const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                       const std::string& token) {
    drogon::orm::DbClientPtr db;
    try {
        db = drogon::app().getDbClient();
    } catch (const std::exception& e) {
        LOG_ERROR << "previewInvite getDbClient: " << e.what();
        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
    }

    db->execSqlAsync(
        "SELECT i.chat_id, i.revoked_at, c.type, c.title, c.description, "
        "       (SELECT COUNT(*) FROM chat_members cm WHERE cm.chat_id = c.id) AS member_count "
        "FROM invites i "
        "JOIN chats c ON c.id = i.chat_id "
        "WHERE i.token = $1",
        [cb](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Invite not found", drogon::k404NotFound));

            const auto& row = r[0];
            if (!row["revoked_at"].isNull())
                return cb(jsonErr("Invite has been revoked", drogon::k410Gone));

            Json::Value resp;
            resp["chat_id"]      = Json::Int64(row["chat_id"].as<long long>());
            resp["type"]         = row["type"].as<std::string>();
            resp["title"]        = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
            resp["description"]  = row["description"].isNull() ? Json::Value() : Json::Value(row["description"].as<std::string>());
            resp["member_count"] = Json::Int64(row["member_count"].as<long long>());
            cb(drogon::HttpResponse::newHttpJsonResponse(resp));
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "previewInvite: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, token);
}

// POST /invites/{token}/join (requires AuthFilter)
void InvitesController::joinInvite(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                    const std::string& token) {
    long long me;
    try {
        me = req->getAttributes()->get<long long>("user_id");
    } catch (...) {
        return cb(jsonErr("Unauthorized", drogon::k401Unauthorized));
    }

    drogon::orm::DbClientPtr db;
    try {
        db = drogon::app().getDbClient();
    } catch (const std::exception& e) {
        LOG_ERROR << "joinInvite getDbClient: " << e.what();
        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
    }

    db->execSqlAsync(
        "SELECT i.chat_id, i.revoked_at FROM invites i WHERE i.token = $1",
        [cb, me](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Invite not found", drogon::k404NotFound));

            const auto& row = r[0];
            if (!row["revoked_at"].isNull())
                return cb(jsonErr("Invite has been revoked", drogon::k410Gone));

            long long chatId = row["chat_id"].as<long long>();

            // Check if already a member
            drogon::orm::DbClientPtr db2;
            try { db2 = drogon::app().getDbClient(); }
            catch (const std::exception& ex) {
                LOG_ERROR << "joinInvite inner getDbClient: " << ex.what();
                return cb(jsonErr("Internal error", drogon::k500InternalServerError));
            }
            db2->execSqlAsync(
                "SELECT role FROM chat_members WHERE chat_id = $1 AND user_id = $2",
                [cb, chatId, me](const drogon::orm::Result& mr) mutable {
                    if (!mr.empty()) {
                        Json::Value resp;
                        resp["chat_id"]         = Json::Int64(chatId);
                        resp["role"]            = mr[0]["role"].as<std::string>();
                        resp["already_member"]  = true;
                        cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                        return;
                    }

                    drogon::orm::DbClientPtr db3;
                    try { db3 = drogon::app().getDbClient(); }
                    catch (const std::exception& ex) {
                        LOG_ERROR << "joinInvite inner getDbClient(3): " << ex.what();
                        return cb(jsonErr("Internal error", drogon::k500InternalServerError));
                    }
                    db3->execSqlAsync(
                        "INSERT INTO chat_members (chat_id, user_id, role) VALUES ($1, $2, 'member') "
                        "ON CONFLICT DO NOTHING",
                        [cb, chatId, me](const drogon::orm::Result&) mutable {
                            // Look up joining user's info and notify chat
                            auto db4 = drogon::app().getDbClient();
                            db4->execSqlAsync(
                                "SELECT username, COALESCE(display_name, username) AS display_name FROM users WHERE id = $1",
                                [cb, chatId, me](const drogon::orm::Result& ur) mutable {
                                    std::string username, displayName;
                                    if (!ur.empty()) {
                                        username = ur[0]["username"].as<std::string>();
                                        displayName = ur[0]["display_name"].as<std::string>();
                                    }
                                    // Broadcast member_joined to chat
                                    Json::Value wsPayload;
                                    wsPayload["type"]         = "chat_member_joined";
                                    wsPayload["chat_id"]      = Json::Int64(chatId);
                                    wsPayload["user_id"]      = Json::Int64(me);
                                    wsPayload["username"]     = username;
                                    wsPayload["display_name"] = displayName;
                                    WsDispatch::publishMessage(chatId, wsPayload);
                                    // Also notify the joining user via their user channel
                                    // so they can subscribe to the chat
                                    Json::Value userPayload;
                                    userPayload["type"]    = "chat_created";
                                    userPayload["chat_id"] = Json::Int64(chatId);
                                    userPayload["chat_type"] = "group";
                                    userPayload["created_by"] = Json::Int64(me);
                                    WsDispatch::publishToUser(me, userPayload);

                                    Json::Value resp;
                                    resp["chat_id"]        = Json::Int64(chatId);
                                    resp["role"]           = "member";
                                    resp["already_member"] = false;
                                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                                },
                                [cb, chatId](const drogon::orm::DrogonDbException&) mutable {
                                    // Even if user lookup fails, the join succeeded
                                    Json::Value resp;
                                    resp["chat_id"]        = Json::Int64(chatId);
                                    resp["role"]           = "member";
                                    resp["already_member"] = false;
                                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                                }, me);
                        },
                        [cb](const drogon::orm::DrogonDbException& e) mutable {
                            LOG_ERROR << "joinInvite insert: " << e.base().what();
                            cb(jsonErr("Internal error", drogon::k500InternalServerError));
                        }, chatId, me);
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "joinInvite check member: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId, me);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "joinInvite: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, token);
}
