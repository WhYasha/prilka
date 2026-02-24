#include "AdminMessagesController.h"
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

void AdminMessagesController::listMessages(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {

    int page = 1, perPage = 50;
    auto pStr = req->getParameter("page");
    auto ppStr = req->getParameter("per_page");
    if (!pStr.empty()) page = std::max(1, std::stoi(pStr));
    if (!ppStr.empty()) perPage = std::clamp(std::stoi(ppStr), 1, 100);

    std::string filterUser = req->getParameter("user_id");
    std::string filterChat = req->getParameter("chat_id");
    std::string filterText = req->getParameter("q");
    int offset = (page - 1) * perPage;

    // Build dynamic WHERE clause
    std::string whereClause = "TRUE";
    std::vector<std::string> paramVals;
    int paramIdx = 0;

    long long filterUserId = 0, filterChatId = 0;
    if (!filterUser.empty()) {
        try { filterUserId = std::stoll(filterUser); } catch (...) {}
    }
    if (!filterChat.empty()) {
        try { filterChatId = std::stoll(filterChat); } catch (...) {}
    }

    if (filterUserId > 0) {
        paramIdx++;
        whereClause += " AND m.sender_id = $" + std::to_string(paramIdx);
    }
    if (filterChatId > 0) {
        paramIdx++;
        whereClause += " AND m.chat_id = $" + std::to_string(paramIdx);
    }
    std::string searchPattern;
    if (!filterText.empty()) {
        searchPattern = "%" + filterText + "%";
        paramIdx++;
        whereClause += " AND m.content ILIKE $" + std::to_string(paramIdx);
    }

    std::string limitP = "$" + std::to_string(paramIdx + 1);
    std::string offsetP = "$" + std::to_string(paramIdx + 2);

    std::string countSql = "SELECT COUNT(*) AS cnt FROM messages m WHERE " + whereClause;
    std::string dataSql =
        "SELECT m.id, m.chat_id, m.sender_id, m.content, m.message_type, m.created_at, "
        "u.username AS sender_username, u.display_name AS sender_display_name, "
        "c.name AS chat_name, c.type::TEXT AS chat_type "
        "FROM messages m "
        "JOIN users u ON u.id = m.sender_id "
        "JOIN chats c ON c.id = m.chat_id "
        "WHERE " + whereClause +
        " ORDER BY m.created_at DESC LIMIT " + limitP + " OFFSET " + offsetP;

    auto cbSh = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
    auto db = drogon::app().getDbClient();

    // Build a lambda that executes both queries with the right params
    // We need to dynamically pass params — use a helper with variants
    // Simplest approach: build all possible combinations

    auto buildMessages = [](const drogon::orm::Result& r2) {
        Json::Value arr(Json::arrayValue);
        for (const auto& row : r2) {
            Json::Value m;
            m["id"]                   = Json::Int64(row["id"].as<long long>());
            m["chat_id"]              = Json::Int64(row["chat_id"].as<long long>());
            m["sender_id"]            = Json::Int64(row["sender_id"].as<long long>());
            m["content"]              = row["content"].isNull() ? Json::Value() : Json::Value(row["content"].as<std::string>());
            m["message_type"]         = row["message_type"].as<std::string>();
            m["created_at"]           = row["created_at"].as<std::string>();
            m["sender_username"]      = row["sender_username"].as<std::string>();
            m["sender_display_name"]  = row["sender_display_name"].isNull() ? Json::Value() : Json::Value(row["sender_display_name"].as<std::string>());
            m["chat_name"]            = row["chat_name"].isNull() ? Json::Value() : Json::Value(row["chat_name"].as<std::string>());
            m["chat_type"]            = row["chat_type"].as<std::string>();
            arr.append(m);
        }
        return arr;
    };

    auto onErr = [cbSh](const drogon::orm::DrogonDbException& e) {
        LOG_ERROR << "admin messages: " << e.base().what();
        (*cbSh)(errResp("Internal error", drogon::k500InternalServerError));
    };

    // Use a generic approach: execute with the right parameter pack based on what filters are active
    // Since Drogon's execSqlAsync takes variadic params, we handle combinations

    // Helper macro-like lambdas for count+data chain
    auto respond = [cbSh, buildMessages, page, perPage](long long total, const drogon::orm::Result& r2) {
        long long totalPages = std::max(1LL, (total + perPage - 1) / perPage);
        Json::Value resp;
        resp["messages"]    = buildMessages(r2);
        resp["page"]        = page;
        resp["per_page"]    = perPage;
        resp["total"]       = Json::Int64(total);
        resp["total_pages"] = Json::Int64(totalPages);
        (*cbSh)(jsonResp(resp, drogon::k200OK));
    };

    // Determine which combination of filters we have
    bool hasUser = filterUserId > 0;
    bool hasChat = filterChatId > 0;
    bool hasText = !searchPattern.empty();

    if (!hasUser && !hasChat && !hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, perPage, offset);
            }, onErr);
    } else if (hasUser && !hasChat && !hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterUserId](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterUserId, perPage, offset);
            }, onErr, filterUserId);
    } else if (!hasUser && hasChat && !hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterChatId](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterChatId, perPage, offset);
            }, onErr, filterChatId);
    } else if (!hasUser && !hasChat && hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, searchPattern](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, searchPattern, perPage, offset);
            }, onErr, searchPattern);
    } else if (hasUser && hasChat && !hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterUserId, filterChatId](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterUserId, filterChatId, perPage, offset);
            }, onErr, filterUserId, filterChatId);
    } else if (hasUser && !hasChat && hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterUserId, searchPattern](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterUserId, searchPattern, perPage, offset);
            }, onErr, filterUserId, searchPattern);
    } else if (!hasUser && hasChat && hasText) {
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterChatId, searchPattern](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterChatId, searchPattern, perPage, offset);
            }, onErr, filterChatId, searchPattern);
    } else {
        // all three filters
        db->execSqlAsync(countSql,
            [db, dataSql, respond, onErr, perPage, offset, filterUserId, filterChatId, searchPattern](const drogon::orm::Result& r) {
                long long total = r[0]["cnt"].as<long long>();
                db->execSqlAsync(dataSql,
                    [respond, total](const drogon::orm::Result& r2) { respond(total, r2); },
                    onErr, filterUserId, filterChatId, searchPattern, perPage, offset);
            }, onErr, filterUserId, filterChatId, searchPattern);
    }
}
