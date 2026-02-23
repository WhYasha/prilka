#include "InvitesController.h"
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
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();

    // Check chat type and user role
    db->execSqlAsync(
        "SELECT c.type, cm.role FROM chats c "
        "JOIN chat_members cm ON cm.chat_id = c.id AND cm.user_id = $2 "
        "WHERE c.id = $1",
        [cb = std::move(cb), chatId, me](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));

            std::string chatType = r[0]["type"].as<std::string>();
            std::string role = r[0]["role"].as<std::string>();

            if (chatType == "direct")
                return cb(jsonErr("Cannot create invites for direct chats", drogon::k400BadRequest));

            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can create invites", drogon::k403Forbidden));

            // Insert invite
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "INSERT INTO invites (chat_id, created_by) VALUES ($1, $2) "
                "RETURNING token, chat_id, created_by, created_at",
                [cb = std::move(cb)](const drogon::orm::Result& ir) mutable {
                    const auto& row = ir[0];
                    std::string token = row["token"].as<std::string>();

                    Json::Value resp;
                    resp["token"]      = token;
                    resp["chat_id"]    = Json::Int64(row["chat_id"].as<long long>());
                    resp["created_by"] = Json::Int64(row["created_by"].as<long long>());
                    resp["created_at"] = row["created_at"].as<std::string>();
                    resp["link"]       = "/invite/" + token;

                    auto httpResp = drogon::HttpResponse::newHttpJsonResponse(resp);
                    httpResp->setStatusCode(drogon::k201Created);
                    cb(httpResp);
                },
                [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "createInvite: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId, me);
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "createInvite check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// GET /chats/{id}/invites
void InvitesController::listInvites(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                     long long chatId) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();

    // Check user role
    db->execSqlAsync(
        "SELECT cm.role FROM chat_members cm WHERE cm.chat_id = $1 AND cm.user_id = $2",
        [cb = std::move(cb), chatId](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Chat not found or access denied", drogon::k404NotFound));

            std::string role = r[0]["role"].as<std::string>();
            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can list invites", drogon::k403Forbidden));

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT token, chat_id, created_by, created_at FROM invites "
                "WHERE chat_id = $1 AND revoked_at IS NULL "
                "ORDER BY created_at DESC",
                [cb = std::move(cb)](const drogon::orm::Result& ir) mutable {
                    Json::Value arr(Json::arrayValue);
                    for (auto& row : ir) {
                        Json::Value inv;
                        std::string token = row["token"].as<std::string>();
                        inv["token"]      = token;
                        inv["chat_id"]    = Json::Int64(row["chat_id"].as<long long>());
                        inv["created_by"] = Json::Int64(row["created_by"].as<long long>());
                        inv["created_at"] = row["created_at"].as<std::string>();
                        inv["link"]       = "/invite/" + token;
                        arr.append(inv);
                    }
                    cb(drogon::HttpResponse::newHttpJsonResponse(arr));
                },
                [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "listInvites: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, chatId);
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "listInvites check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, chatId, me);
}

// DELETE /invites/{token}
void InvitesController::revokeInvite(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      const std::string& token) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();

    // Find invite and check ownership
    db->execSqlAsync(
        "SELECT i.chat_id, cm.role FROM invites i "
        "JOIN chat_members cm ON cm.chat_id = i.chat_id AND cm.user_id = $2 "
        "WHERE i.token = $1 AND i.revoked_at IS NULL",
        [cb = std::move(cb), token](const drogon::orm::Result& r) mutable {
            if (r.empty())
                return cb(jsonErr("Invite not found or already revoked", drogon::k404NotFound));

            std::string role = r[0]["role"].as<std::string>();
            if (role != "owner" && role != "admin")
                return cb(jsonErr("Only owner or admin can revoke invites", drogon::k403Forbidden));

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE invites SET revoked_at = NOW() WHERE token = $1",
                [cb = std::move(cb)](const drogon::orm::Result&) mutable {
                    Json::Value resp;
                    resp["revoked"] = true;
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                },
                [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "revokeInvite: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, token);
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "revokeInvite check: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, token, me);
}
