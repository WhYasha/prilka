#include "WsHandler.h"
#include "../services/JwtService.h"
#include "../services/MetricsService.h"
#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>

// ── Static members ─────────────────────────────────────────────────────────
std::mutex WsHandler::s_mu;
std::unordered_map<long long, std::vector<drogon::WebSocketConnectionPtr>> WsHandler::s_subs;
std::unordered_map<long long, bool> WsHandler::s_redisSubs;

// ── Helpers ────────────────────────────────────────────────────────────────

static Json::Value parseJson(const std::string& s) {
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::istringstream ss(s);
    std::string err;
    Json::parseFromStream(rb, ss, &root, &err);
    return root;
}

static std::string toJsonStr(const Json::Value& v) {
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    return Json::writeString(wb, v);
}

static void sendJson(const drogon::WebSocketConnectionPtr& conn,
                     const Json::Value& payload) {
    if (conn && !conn->disconnected())
        conn->send(toJsonStr(payload));
}

static void sendError(const drogon::WebSocketConnectionPtr& conn,
                      const std::string& msg) {
    Json::Value e;
    e["type"]    = "error";
    e["message"] = msg;
    sendJson(conn, e);
}

// ── Broadcast to local subscribers ────────────────────────────────────────

void WsHandler::broadcast(long long chatId, const Json::Value& payload) {
    std::lock_guard<std::mutex> lk(s_mu);
    auto it = s_subs.find(chatId);
    if (it == s_subs.end()) return;
    std::string msg = toJsonStr(payload);
    for (auto& conn : it->second) {
        if (conn && !conn->disconnected()) conn->send(msg);
    }
}

// ── Redis subscription ─────────────────────────────────────────────────────

void WsHandler::subscribeToRedis(long long chatId) {
    {
        std::lock_guard<std::mutex> lk(s_mu);
        if (s_redisSubs.count(chatId)) return;
        s_redisSubs[chatId] = true;
    }

    std::string channel = "chat:" + std::to_string(chatId);
    auto redis = drogon::app().getRedisClient();
    if (!redis) {
        LOG_WARN << "Redis not configured; WebSocket fan-out is local-only";
        return;
    }

    // Subscribe to Redis channel; when a message arrives, broadcast locally.
    redis->newSubscriber()->subscribe(
        channel,
        [chatId](const std::string& /*channel*/, const std::string& msg) {
            auto payload = parseJson(msg);
            WsHandler::broadcast(chatId, payload);
        }
    );
}

// ── WebSocket lifecycle ────────────────────────────────────────────────────

void WsHandler::handleNewConnection(const drogon::HttpRequestPtr& req,
                                    const drogon::WebSocketConnectionPtr& conn) {
    MetricsService::instance().wsConnect();
    auto ctx = std::make_shared<ConnCtx>();
    conn->setContext(ctx);
    LOG_INFO << "WebSocket opened from " << conn->peerAddr().toIpPort();
}

void WsHandler::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    MetricsService::instance().wsDisconnect();
    auto ctx = conn->getContext<ConnCtx>();
    if (ctx) {
        std::lock_guard<std::mutex> lk(s_mu);
        for (long long chatId : ctx->subscriptions) {
            auto& vec = s_subs[chatId];
            vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
        }
    }
    LOG_INFO << "WebSocket closed";
}

void WsHandler::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                  std::string&&                         rawMsg,
                                  const drogon::WebSocketMessageType&   msgType) {
    if (msgType == drogon::WebSocketMessageType::Ping) {
        conn->send("", drogon::WebSocketMessageType::Pong);
        return;
    }
    if (msgType != drogon::WebSocketMessageType::Text) return;

    auto msg = parseJson(rawMsg);
    if (msg.isNull()) { sendError(conn, "Invalid JSON"); return; }

    std::string type = msg["type"].asString();
    auto ctx = conn->getContext<ConnCtx>();
    if (!ctx) { conn->forceClose(); return; }

    // ── auth ───────────────────────────────────────────────────────────────
    if (type == "auth") {
        std::string token = msg["token"].asString();
        auto claims = JwtService::instance().verify(token);
        if (!claims || claims->tokenType != "access") {
            sendError(conn, "Invalid or expired access token");
            conn->forceClose();
            return;
        }
        ctx->userId = claims->userId;
        ctx->authed = true;

        // Update last_activity for admin dashboard metrics
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "UPDATE users SET last_activity = NOW() WHERE id = $1",
            [](const drogon::orm::Result&) {},
            [](const drogon::orm::DrogonDbException&) {},
            ctx->userId);

        Json::Value ok;
        ok["type"]    = "auth_ok";
        ok["user_id"] = Json::Int64(ctx->userId);
        sendJson(conn, ok);
        return;
    }

    if (!ctx->authed) { sendError(conn, "Not authenticated"); return; }

    // ── ping ───────────────────────────────────────────────────────────────
    if (type == "ping") {
        Json::Value pong; pong["type"] = "pong";
        sendJson(conn, pong);
        return;
    }

    // ── subscribe ──────────────────────────────────────────────────────────
    if (type == "subscribe") {
        long long chatId = msg["chat_id"].asInt64();
        if (chatId <= 0) { sendError(conn, "Invalid chat_id"); return; }

        long long userId = ctx->userId;
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT 1 FROM chat_members WHERE chat_id = $1 AND user_id = $2",
            [this, conn, ctx, chatId](const drogon::orm::Result& r) {
                if (conn->disconnected()) return;
                if (r.empty()) {
                    sendError(conn, "Not a member of this chat");
                    return;
                }
                {
                    std::lock_guard<std::mutex> lk(s_mu);
                    auto& vec = s_subs[chatId];
                    if (std::find(vec.begin(), vec.end(), conn) == vec.end()) {
                        vec.push_back(conn);
                        ctx->subscriptions.push_back(chatId);
                    }
                }
                subscribeToRedis(chatId);
                Json::Value ok;
                ok["type"]    = "subscribed";
                ok["chat_id"] = Json::Int64(chatId);
                sendJson(conn, ok);
            },
            [conn](const drogon::orm::DrogonDbException& e) {
                if (conn->disconnected()) return;
                LOG_ERROR << "WS subscribe membership check: " << e.base().what();
                sendError(conn, "Internal error");
            },
            chatId, userId);
        return;
    }

    sendError(conn, "Unknown message type: " + type);
}

// ── Static helper called from MessagesController after DB insert ───────────
// Publishes to Redis → all nodes pick it up and fan-out locally.
namespace WsDispatch {
void publishMessage(long long chatId, const Json::Value& payload) {
    auto redis = drogon::app().getRedisClient();
    std::string channel = "chat:" + std::to_string(chatId);
    std::string msg = ([&] {
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        return Json::writeString(wb, payload);
    })();

    if (redis) {
        redis->execCommandAsync(
            [](const drogon::nosql::RedisResult&) {},
            [](const std::exception& e) {
                LOG_ERROR << "Redis PUBLISH error: " << e.what();
            },
            "PUBLISH %s %s", channel.c_str(), msg.c_str()
        );
    } else {
        // Fallback: local broadcast only
        WsHandler::broadcast(chatId, payload);
    }
}
}  // namespace WsDispatch
