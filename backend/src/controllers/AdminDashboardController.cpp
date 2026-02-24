#include "AdminDashboardController.h"
#include <trantor/utils/Logger.h>

static drogon::HttpResponsePtr jsonResp(const Json::Value& body, drogon::HttpStatusCode code) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}

static drogon::HttpResponsePtr errResp(const std::string& msg, drogon::HttpStatusCode code) {
    Json::Value b; b["error"] = msg;
    return jsonResp(b, code);
}

void AdminDashboardController::getStats(
        const drogon::HttpRequestPtr& /*req*/,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    auto result = std::make_shared<Json::Value>();
    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    // Counter to track how many queries have completed
    auto pending = std::make_shared<std::atomic<int>>(11);

    auto db = drogon::app().getDbClient();
    auto onError = [cbSh, pending](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "admin stats DB error: " << e.base().what();
        int prev = pending->fetch_sub(1);
        if (prev == 1) {
            (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
        }
    };

    auto maybeRespond = [cbSh, result, pending]() {
        int prev = pending->fetch_sub(1);
        if (prev == 1) {
            (*cbSh)(jsonResp(*result, drogon::k200OK));
        }
    };

    // 1. total_users
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM users WHERE is_active = TRUE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["total_users"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 2. dau
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM users WHERE last_activity >= NOW() - INTERVAL '24 hours' AND is_active = TRUE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["dau"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 3. wau
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM users WHERE last_activity >= NOW() - INTERVAL '7 days' AND is_active = TRUE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["wau"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 4. total_chats
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM chats",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["total_chats"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 5. chats_by_type
    db->execSqlAsync(
        "SELECT type::TEXT, COUNT(*) AS cnt FROM chats GROUP BY type ORDER BY type",
        [result, maybeRespond](const drogon::orm::Result& r) {
            Json::Value obj(Json::objectValue);
            for (const auto& row : r) {
                obj[row["type"].as<std::string>()] = Json::Int64(row["cnt"].as<long long>());
            }
            (*result)["chats_by_type"] = obj;
            maybeRespond();
        }, onError);

    // 6. total_messages
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM messages",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["total_messages"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 7. messages_today
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM messages WHERE created_at >= CURRENT_DATE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["messages_today"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 8. new_users_today
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM users WHERE created_at >= CURRENT_DATE AND is_active = TRUE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["new_users_today"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 9. blocked_users
    db->execSqlAsync(
        "SELECT COUNT(*) AS cnt FROM users WHERE is_blocked = TRUE",
        [result, maybeRespond](const drogon::orm::Result& r) {
            (*result)["blocked_users"] = Json::Int64(r[0]["cnt"].as<long long>());
            maybeRespond();
        }, onError);

    // 10. reg_trend (14 days)
    db->execSqlAsync(
        "SELECT DATE(created_at)::TEXT AS day, COUNT(*) AS cnt "
        "FROM users WHERE created_at >= CURRENT_DATE - INTERVAL '14 days' AND is_active = TRUE "
        "GROUP BY day ORDER BY day",
        [result, maybeRespond](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value item;
                item["day"]   = row["day"].as<std::string>();
                item["count"] = Json::Int64(row["cnt"].as<long long>());
                arr.append(item);
            }
            (*result)["reg_trend"] = arr;
            maybeRespond();
        }, onError);

    // 11. msg_trend (14 days)
    db->execSqlAsync(
        "SELECT DATE(created_at)::TEXT AS day, COUNT(*) AS cnt "
        "FROM messages WHERE created_at >= CURRENT_DATE - INTERVAL '14 days' "
        "GROUP BY day ORDER BY day",
        [result, maybeRespond](const drogon::orm::Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value item;
                item["day"]   = row["day"].as<std::string>();
                item["count"] = Json::Int64(row["cnt"].as<long long>());
                arr.append(item);
            }
            (*result)["msg_trend"] = arr;
            maybeRespond();
        }, onError);
}
