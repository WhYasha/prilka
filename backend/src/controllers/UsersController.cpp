#include "UsersController.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr userJson(const drogon::orm::Row& r) {
    Json::Value u;
    u["id"]           = Json::Int64(r["id"].as<long long>());
    u["username"]     = r["username"].as<std::string>();
    u["display_name"] = r["display_name"].as<std::string>();
    u["bio"]          = r["bio"].isNull() ? Json::Value() : Json::Value(r["bio"].as<std::string>());
    return drogon::HttpResponse::newHttpJsonResponse(u);
}

static drogon::HttpResponsePtr notFound() {
    Json::Value b; b["error"] = "User not found";
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(drogon::k404NotFound);
    return r;
}

// GET /users/{id}
void UsersController::getUser(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                               long long userId) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, username, display_name, bio FROM users WHERE id = $1 AND is_active = TRUE",
        [cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            if (r.empty()) return cb(notFound());
            cb(userJson(r[0]));
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "getUser: " << e.base().what();
            Json::Value b; b["error"] = "Internal error";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(b);
            resp->setStatusCode(drogon::k500InternalServerError);
            cb(resp);
        }, userId);
}

// GET /users/search?q=alice
void UsersController::searchUsers(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    std::string q = req->getParameter("q");
    if (q.empty()) {
        Json::Value b; b["error"] = "q parameter required";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(b);
        resp->setStatusCode(drogon::k400BadRequest);
        return cb(resp);
    }

    std::string pattern = "%" + q + "%";
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, username, display_name, bio FROM users "
        "WHERE (username ILIKE $1 OR display_name ILIKE $1) AND is_active = TRUE "
        "ORDER BY username LIMIT 20",
        [cb = std::move(cb)](const drogon::orm::Result& r) mutable {
            Json::Value arr(Json::arrayValue);
            for (auto& row : r) {
                Json::Value u;
                u["id"]           = Json::Int64(row["id"].as<long long>());
                u["username"]     = row["username"].as<std::string>();
                u["display_name"] = row["display_name"].as<std::string>();
                arr.append(u);
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(arr));
        },
        [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "searchUsers: " << e.base().what();
            Json::Value b; b["error"] = "Internal error";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(b);
            resp->setStatusCode(drogon::k500InternalServerError);
            cb(resp);
        }, pattern);
}
