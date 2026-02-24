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
                "SELECT theme, notifications_enabled, language, last_seen_visibility FROM user_settings WHERE user_id = $1",
                [cb](const drogon::orm::Result& r) mutable {
                    if (r.empty()) {
                        // Fallback defaults
                        Json::Value resp;
                        resp["theme"]                 = "light";
                        resp["notifications_enabled"] = true;
                        resp["language"]              = "en";
                        resp["last_seen_visibility"]  = "everyone";
                        cb(drogon::HttpResponse::newHttpJsonResponse(resp));
                        return;
                    }
                    Json::Value resp;
                    resp["theme"]                 = r[0]["theme"].as<std::string>();
                    resp["notifications_enabled"] = r[0]["notifications_enabled"].as<bool>();
                    resp["language"]              = r[0]["language"].isNull() ? "en" : r[0]["language"].as<std::string>();
                    resp["last_seen_visibility"]  = r[0]["last_seen_visibility"].isNull() ? "everyone" : r[0]["last_seen_visibility"].as<std::string>();
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
                "SELECT theme, notifications_enabled, language, last_seen_visibility FROM user_settings WHERE user_id = $1",
                [cb](const drogon::orm::Result& r) mutable {
                    Json::Value resp;
                    resp["theme"]                 = r.empty() ? "light" : r[0]["theme"].as<std::string>();
                    resp["notifications_enabled"] = r.empty() ? true    : r[0]["notifications_enabled"].as<bool>();
                    resp["language"]              = r.empty() ? "en"    : (r[0]["language"].isNull() ? "en" : r[0]["language"].as<std::string>());
                    resp["last_seen_visibility"]  = r.empty() ? "everyone" : (r[0]["last_seen_visibility"].isNull() ? "everyone" : r[0]["last_seen_visibility"].as<std::string>());
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
    std::string language = (*body).get("language", "en").asString();
    std::string lastSeenVisibility = (*body).get("last_seen_visibility", "everyone").asString();

    if (theme != "light" && theme != "dark") theme = "light";
    // Basic language validation — only allow known codes
    if (language != "en" && language != "ru" && language != "de") language = "en";
    // Validate last_seen_visibility
    if (lastSeenVisibility != "everyone" && lastSeenVisibility != "nobody" && lastSeenVisibility != "approx_only")
        lastSeenVisibility = "everyone";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO user_settings (user_id, theme, notifications_enabled, language, last_seen_visibility) "
        "VALUES ($1, $2, $3, $4, $5) "
        "ON CONFLICT (user_id) DO UPDATE "
        "SET theme = EXCLUDED.theme, notifications_enabled = EXCLUDED.notifications_enabled, "
        "    language = EXCLUDED.language, last_seen_visibility = EXCLUDED.last_seen_visibility",
        [cb](const drogon::orm::Result&) mutable {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            cb(resp);
        },
        [cb](const drogon::orm::DrogonDbException& e) mutable {
            LOG_ERROR << "putSettings: " << e.base().what();
            cb(jsonErr("Internal error", drogon::k500InternalServerError));
        }, me, theme, notifications, language, lastSeenVisibility);
}
