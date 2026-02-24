#include "AdminUsersController.h"
#include <trantor/utils/Logger.h>
#include <sstream>

static drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}

static drogon::HttpResponsePtr errResp(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    return jsonResp(b, code);
}

// ── GET /admin-api/users ────────────────────────────────────────────────────

void AdminUsersController::listUsers(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    int page = 1, perPage = 20;
    auto pStr = req->getParameter("page");
    auto ppStr = req->getParameter("per_page");
    if (!pStr.empty()) page = std::max(1, std::stoi(pStr));
    if (!ppStr.empty()) perPage = std::clamp(std::stoi(ppStr), 1, 100);

    std::string search = req->getParameter("q");
    std::string status = req->getParameter("status");
    int offset = (page - 1) * perPage;

    // Build WHERE clause with positional parameters
    std::string whereClause = "TRUE";
    std::vector<std::string> params;
    int paramIdx = 0;

    std::string searchPattern;
    if (!search.empty()) {
        searchPattern = "%" + search + "%";
        paramIdx++;
        std::string p1 = "$" + std::to_string(paramIdx);
        paramIdx++;
        std::string p2 = "$" + std::to_string(paramIdx);
        whereClause += " AND (u.username ILIKE " + p1 + " OR u.display_name ILIKE " + p2 + ")";
    }

    if (status == "active") whereClause += " AND u.is_active = TRUE AND u.is_blocked = FALSE";
    else if (status == "blocked") whereClause += " AND u.is_blocked = TRUE";
    else if (status == "admin") whereClause += " AND u.is_admin = TRUE";
    else if (status == "inactive") whereClause += " AND u.is_active = FALSE";

    std::string limitParam = "$" + std::to_string(paramIdx + 1);
    std::string offsetParam = "$" + std::to_string(paramIdx + 2);

    std::string countSql = "SELECT COUNT(*) AS cnt FROM users u WHERE " + whereClause;
    std::string dataSql =
        "SELECT u.id, u.username, u.display_name, u.email, u.is_active, "
        "u.is_admin, u.is_blocked, u.created_at, u.last_activity "
        "FROM users u WHERE " + whereClause +
        " ORDER BY u.created_at DESC LIMIT " + limitParam + " OFFSET " + offsetParam;

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();

    if (!search.empty()) {
        // With search params
        db->execSqlAsync(countSql,
            [cbSh, db, dataSql, searchPattern, perPage, offset, page](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                long long totalPages = std::max(1LL, (total + perPage - 1) / perPage);
                db->execSqlAsync(dataSql,
                    [cbSh, total, totalPages, page, perPage](const drogon::orm::Result& r2) {
                        Json::Value users(Json::arrayValue);
                        for (const auto& row : r2) {
                            Json::Value u;
                            u["id"]            = Json::Int64(row["id"].as<long long>());
                            u["username"]      = row["username"].as<std::string>();
                            u["display_name"]  = row["display_name"].isNull() ? Json::Value() : Json::Value(row["display_name"].as<std::string>());
                            u["email"]         = row["email"].isNull() ? Json::Value() : Json::Value(row["email"].as<std::string>());
                            u["is_active"]     = row["is_active"].as<bool>();
                            u["is_admin"]      = row["is_admin"].isNull() ? false : row["is_admin"].as<bool>();
                            u["is_blocked"]    = row["is_blocked"].as<bool>();
                            u["created_at"]    = row["created_at"].as<std::string>();
                            u["last_activity"] = row["last_activity"].isNull() ? Json::Value() : Json::Value(row["last_activity"].as<std::string>());
                            users.append(u);
                        }
                        Json::Value resp;
                        resp["users"]       = users;
                        resp["page"]        = page;
                        resp["per_page"]    = perPage;
                        resp["total"]       = Json::Int64(total);
                        resp["total_pages"] = Json::Int64(totalPages);
                        (*cbSh)(jsonResp(resp, drogon::k200OK));
                    },
                    [cbSh](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "admin users list: " << e.base().what();
                        (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                    }, searchPattern, searchPattern, perPage, offset);
            },
            [cbSh](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "admin users count: " << e.base().what();
                (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
            }, searchPattern, searchPattern);
    } else {
        // Without search params
        db->execSqlAsync(countSql,
            [cbSh, db, dataSql, perPage, offset, page](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                long long totalPages = std::max(1LL, (total + perPage - 1) / perPage);
                db->execSqlAsync(dataSql,
                    [cbSh, total, totalPages, page, perPage](const drogon::orm::Result& r2) {
                        Json::Value users(Json::arrayValue);
                        for (const auto& row : r2) {
                            Json::Value u;
                            u["id"]            = Json::Int64(row["id"].as<long long>());
                            u["username"]      = row["username"].as<std::string>();
                            u["display_name"]  = row["display_name"].isNull() ? Json::Value() : Json::Value(row["display_name"].as<std::string>());
                            u["email"]         = row["email"].isNull() ? Json::Value() : Json::Value(row["email"].as<std::string>());
                            u["is_active"]     = row["is_active"].as<bool>();
                            u["is_admin"]      = row["is_admin"].isNull() ? false : row["is_admin"].as<bool>();
                            u["is_blocked"]    = row["is_blocked"].as<bool>();
                            u["created_at"]    = row["created_at"].as<std::string>();
                            u["last_activity"] = row["last_activity"].isNull() ? Json::Value() : Json::Value(row["last_activity"].as<std::string>());
                            users.append(u);
                        }
                        Json::Value resp;
                        resp["users"]       = users;
                        resp["page"]        = page;
                        resp["per_page"]    = perPage;
                        resp["total"]       = Json::Int64(total);
                        resp["total_pages"] = Json::Int64(totalPages);
                        (*cbSh)(jsonResp(resp, drogon::k200OK));
                    },
                    [cbSh](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "admin users list: " << e.base().what();
                        (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
                    }, perPage, offset);
            },
            [cbSh](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "admin users count: " << e.base().what();
                (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
            });
    }
}

// ── GET /admin-api/users/{id} ───────────────────────────────────────────────

void AdminUsersController::getUser(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
        long long userId) {

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto result = std::make_shared<Json::Value>();
    auto pending = std::make_shared<std::atomic<int>>(4);
    auto failed = std::make_shared<std::atomic<bool>>(false);

    auto db = drogon::app().getDbClient();

    auto maybeRespond = [cbSh, result, pending, failed]() {
        int prev = pending->fetch_sub(1);
        if (prev == 1) {
            if (failed->load()) {
                (*cbSh)(errResp("User not found", drogon::k404NotFound));
            } else {
                (*cbSh)(jsonResp(*result, drogon::k200OK));
            }
        }
    };

    auto onError = [cbSh, pending](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "admin user detail: " << e.base().what();
        int prev = pending->fetch_sub(1);
        if (prev == 1)
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
    };

    // 1. User profile
    db->execSqlAsync(
        "SELECT id, username, display_name, email, bio, is_active, is_admin, is_blocked, "
        "created_at, updated_at, last_activity FROM users WHERE id = $1",
        [result, maybeRespond, failed](const drogon::orm::Result& r) {
            if (r.empty()) {
                failed->store(true);
                maybeRespond();
                return;
            }
            const auto& row = r[0];
            Json::Value u;
            u["id"]            = Json::Int64(row["id"].as<long long>());
            u["username"]      = row["username"].as<std::string>();
            u["display_name"]  = row["display_name"].isNull() ? Json::Value() : Json::Value(row["display_name"].as<std::string>());
            u["email"]         = row["email"].isNull() ? Json::Value() : Json::Value(row["email"].as<std::string>());
            u["bio"]           = row["bio"].isNull() ? Json::Value() : Json::Value(row["bio"].as<std::string>());
            u["is_active"]     = row["is_active"].as<bool>();
            u["is_admin"]      = row["is_admin"].isNull() ? false : row["is_admin"].as<bool>();
            u["is_blocked"]    = row["is_blocked"].as<bool>();
            u["created_at"]    = row["created_at"].as<std::string>();
            u["updated_at"]    = row["updated_at"].isNull() ? Json::Value() : Json::Value(row["updated_at"].as<std::string>());
            u["last_activity"] = row["last_activity"].isNull() ? Json::Value() : Json::Value(row["last_activity"].as<std::string>());
            (*result)["user"] = u;
            maybeRespond();
        }, onError, userId);

    // 2. Message count
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM messages WHERE sender_id = $1",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["message_count"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError, userId);

    // 3. Chats
    db->execSqlAsync(
        "SELECT c.id, c.type::TEXT AS type, c.name, c.title, cm.role, cm.joined_at, "
        "(SELECT COUNT(*) FROM chat_members WHERE chat_id = c.id) AS member_count "
        "FROM chat_members cm JOIN chats c ON c.id = cm.chat_id "
        "WHERE cm.user_id = $1 ORDER BY cm.joined_at DESC",
        [result, maybeRespond](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value c;
                c["id"]           = Json::Int64(row["id"].as<long long>());
                c["type"]         = row["type"].as<std::string>();
                c["name"]         = row["name"].isNull() ? Json::Value() : Json::Value(row["name"].as<std::string>());
                c["title"]        = row["title"].isNull() ? Json::Value() : Json::Value(row["title"].as<std::string>());
                c["role"]         = row["role"].as<std::string>();
                c["joined_at"]    = row["joined_at"].as<std::string>();
                c["member_count"] = Json::Int64(row["member_count"].as<long long>());
                arr.append(c);
            }
            (*result)["chats"] = arr;
            maybeRespond();
        }, onError, userId);

    // 4. Recent 50 messages
    db->execSqlAsync(
        "SELECT m.id, m.chat_id, m.content, m.message_type, m.created_at, "
        "c.name AS chat_name, c.type::TEXT AS chat_type "
        "FROM messages m JOIN chats c ON c.id = m.chat_id "
        "WHERE m.sender_id = $1 ORDER BY m.created_at DESC LIMIT 50",
        [result, maybeRespond](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value m;
                m["id"]           = Json::Int64(row["id"].as<long long>());
                m["chat_id"]      = Json::Int64(row["chat_id"].as<long long>());
                m["content"]      = row["content"].isNull() ? Json::Value() : Json::Value(row["content"].as<std::string>());
                m["message_type"] = row["message_type"].as<std::string>();
                m["created_at"]   = row["created_at"].as<std::string>();
                m["chat_name"]    = row["chat_name"].isNull() ? Json::Value() : Json::Value(row["chat_name"].as<std::string>());
                m["chat_type"]    = row["chat_type"].as<std::string>();
                arr.append(m);
            }
            (*result)["messages"] = arr;
            maybeRespond();
        }, onError, userId);
}

// ── POST /admin-api/users/{id}/block ────────────────────────────────────────

void AdminUsersController::blockUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
        long long userId) {

    auto me = req->getAttributes()->get<long long>("user_id");
    if (me == userId) return cb(errResp("Cannot block yourself", drogon::k400BadRequest));

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_blocked = TRUE WHERE id = $1 AND is_blocked = FALSE",
        [cbSh](const drogon::orm::Result&) {
            (*cbSh)(jsonResp(Json::Value(Json::objectValue), drogon::k204NoContent));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin block: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        }, userId);
}

// ── POST /admin-api/users/{id}/unblock ──────────────────────────────────────

void AdminUsersController::unblockUser(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
        long long userId) {

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_blocked = FALSE WHERE id = $1 AND is_blocked = TRUE",
        [cbSh](const drogon::orm::Result&) {
            (*cbSh)(jsonResp(Json::Value(Json::objectValue), drogon::k204NoContent));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin unblock: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        }, userId);
}

// ── POST /admin-api/users/{id}/soft-delete ──────────────────────────────────

void AdminUsersController::softDelete(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
        long long userId) {

    auto me = req->getAttributes()->get<long long>("user_id");
    if (me == userId) return cb(errResp("Cannot delete yourself", drogon::k400BadRequest));

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_active = FALSE, is_blocked = TRUE WHERE id = $1",
        [cbSh](const drogon::orm::Result&) {
            (*cbSh)(jsonResp(Json::Value(Json::objectValue), drogon::k204NoContent));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin soft-delete: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        }, userId);
}

// ── POST /admin-api/users/{id}/toggle-admin ─────────────────────────────────

void AdminUsersController::toggleAdmin(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
        long long userId) {

    auto me = req->getAttributes()->get<long long>("user_id");
    if (me == userId) return cb(errResp("Cannot change your own admin status", drogon::k400BadRequest));

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_admin = NOT is_admin WHERE id = $1",
        [cbSh](const drogon::orm::Result&) {
            (*cbSh)(jsonResp(Json::Value(Json::objectValue), drogon::k204NoContent));
        },
        [cbSh](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "admin toggle-admin: " << e.base().what();
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        }, userId);
}
