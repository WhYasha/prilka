#include "SettingsController.h"
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonErr(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    auto r = drogon::HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    return r;
}

// GET /settings
void SettingsController::getSettings(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto db = drogon::app().getDbClient();

    // Ensure a default row exists, then return
    db->execSqlAsync(
        "INSERT INTO user_settings (user_id) VALUES ($1) ON CONFLICT DO NOTHING",
        [me, cb](const drogon::orm::Result&) mutable {
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT theme, notifications_enabled FROM user_settings WHERE user_id = $1",
                [cb](const drogon::orm::Result& r) mutable {
                    if (r.empty()) {
                        // Fallback defaults
                        Json::Value resp;
                        resp["theme"]                 = "light";
                        resp["notifications_enabled"] = true;
                        cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                        return;
                    }
                    Json::Value resp;
                    resp["theme"]                 = r[0]["theme"].as<std::string>();
                    resp["notifications_enabled"] = r[0]["notifications_enabled"].as<bool>();
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                },
                [cb](const drogon::orm::DrogonDbException& e) mutable {
                    LOG_ERROR << "getSettings select: " << e.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, me);
        },
        [me, cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_WARN << "getSettings insert: " << e.base().what();
            // Proceed with select even if insert failed (row exists)
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "SELECT theme, notifications_enabled FROM user_settings WHERE user_id = $1",
                [cb](const drogon::orm::Result& r) mutable {
                    Json::Value resp;
                    resp["theme"]                 = r.empty() ? "light" : r[0]["theme"].as<std::string>();
                    resp["notifications_enabled"] = r.empty() ? true    : r[0]["notifications_enabled"].as<bool>();
                    cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                },
                [cb](const drogon::orm::DrogonDbException& e2) mutable {
                    LOG_ERROR << "getSettings fallback: " << e2.base().what();
                    cb(jsonErr("Internal error", drogon::k500InternalServerError));
                }, me);
        }, me);
}

// PUT /settings
void SettingsController::putSettings(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    long long me = req->getAttributes()->get<long long>("user_id");
    auto body = req->getJsonObject();
    if (!body) return cb(jsonErr("Invalid JSON", drogon::k400BadRequest));

    std::string theme = (*body).get("theme", "light").asString();
    bool notifications = (*body).get("notifications_enabled", true).asBool();

    if (theme != "light" && theme != "dark") theme = "light";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO user_settings (user_id, theme, notifications_enabled) "
        "VALUES ($1, $2, $3) "
        "ON CONFLICT (user_id) DO UPDATE "
        "SET theme = EXCLUDED.theme, notifications_enabled = EXCLUDED.notifications_enabled",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "putSettings: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, theme, notifications);
}
