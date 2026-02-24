#include "WsHandler.h"
#include "../services/JwtService.h"
#include "../services/MetricsService.h"
#include <drogon/nosql/RedisClient.h>
#include <drogon/nosql/RedisSubscriber.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>

// ── Static members ─────────────────────────────────────────────────────────
std::mutex WsHandler::s_mu;
std::unordered_map<long long, std::vector<drogon::WebSocketConnectionPtr>> WsHandler::s_subs;
std::unordered_map<long long, bool> WsHandler::s_redisSubs;
std::mutex WsHandler::s_userMu;
std::unordered_map<long long, std::vector<drogon::WebSocketConnectionPtr>> WsHandler::s_userConns;

// Keep Redis subscriber objects alive so callbacks remain valid
static std::mutex s_redisSubMu;
static std::unordered_map<long long,
       std::shared_ptr<drogon::nosql::RedisSubscriber>> s_redisSubPtrs;

bool WsHandler::isUserOnline(long long userId) {
    std::lock_guard<std::mutex> lk(s_userMu);
    auto it = s_userConns.find(userId);
    return it != s_userConns.end() && !it->second.empty();
}

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
    try {
        std::lock_guard<std::mutex> lk(s_mu);
        auto it = s_subs.find(chatId);
        if (it == s_subs.end()) return;
        std::string msg = toJsonStr(payload);
        for (auto& conn : it->second) {
            try {
                if (conn && !conn->disconnected()) conn->send(msg);
            } catch (const std::exception& e) {
                LOG_ERROR << "WS broadcast send error: " << e.what();
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "WS broadcast error for chat " << chatId << ": " << e.what();
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

    try {
        // Store the subscriber so its callback remains valid for the process lifetime
        auto subscriber = redis->newSubscriber();
        subscriber->subscribe(
            channel,
            [chatId](const std::string& /*channel*/, const std::string& msg) {
                try {
                    auto payload = parseJson(msg);
                    WsHandler::broadcast(chatId, payload);
                } catch (const std::exception& e) {
                    LOG_ERROR << "Redis subscribe callback error for chat "
                              << chatId << ": " << e.what();
                }
            }
        );
        {
            std::lock_guard<std::mutex> lk(s_redisSubMu);
            s_redisSubPtrs[chatId] = std::move(subscriber);
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to subscribe to Redis channel " << channel
                  << ": " << e.what();
        std::lock_guard<std::mutex> lk(s_mu);
        s_redisSubs.erase(chatId);
    }
}

// ── Presence broadcast ────────────────────────────────────────────────────

// Helper: compute approximate last-seen bucket string
static std::string lastSeenBucket(const std::string& status) {
    if (status == "online") return "online";
    // For offline, we don't have exact time here — just report "recently"
    return "recently";
}

void WsHandler::broadcastPresence(long long userId, const std::string& username,
                                   const std::string& status) {
    auto db = drogon::app().getDbClient();

    // First query the user's last_seen_visibility setting
    db->execSqlAsync(
        "SELECT COALESCE(us.last_seen_visibility, 'everyone') AS visibility "
        "FROM users u LEFT JOIN user_settings us ON us.user_id = u.id "
        "WHERE u.id = $1",
        [userId, username, status](const drogon::orm::Result& r) {
            std::string visibility = "everyone";
            if (!r.empty()) {
                visibility = r[0]["visibility"].as<std::string>();
            }

            // Build payloads for different audience types
            Json::Value fullPayload;
            fullPayload["type"]     = "presence";
            fullPayload["user_id"]  = Json::Int64(userId);
            fullPayload["username"] = username;
            fullPayload["status"]   = status;

            Json::Value approxPayload;
            approxPayload["type"]     = "presence";
            approxPayload["user_id"]  = Json::Int64(userId);
            approxPayload["username"] = username;
            approxPayload["privacy"]  = "approx_only";
            approxPayload["last_seen_bucket"] = lastSeenBucket(status);

            if (visibility == "everyone") {
                // Current behavior: broadcast full presence to everyone
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT chat_id FROM chat_members WHERE user_id = $1",
                    [fullPayload](const drogon::orm::Result& r2) {
                        for (const auto& row : r2) {
                            long long chatId = row["chat_id"].as<long long>();
                            WsDispatch::publishMessage(chatId, fullPayload);
                        }
                    },
                    [userId](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "broadcastPresence DB error for user " << userId
                                  << ": " << e.base().what();
                    },
                    userId);
            } else {
                // For 'approx_only' and 'nobody': do local filtered broadcast
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT chat_id FROM chat_members WHERE user_id = $1",
                    [visibility, fullPayload, approxPayload](const drogon::orm::Result& r2) {
                        for (const auto& row : r2) {
                            long long chatId = row["chat_id"].as<long long>();
                            std::string fullMsg   = toJsonStr(fullPayload);
                            std::string approxMsg = toJsonStr(approxPayload);

                            std::lock_guard<std::mutex> lk(s_mu);
                            auto it = s_subs.find(chatId);
                            if (it == s_subs.end()) continue;

                            for (auto& conn : it->second) {
                                try {
                                    if (!conn || conn->disconnected()) continue;
                                    auto ctx = conn->getContext<ConnCtx>();
                                    if (!ctx) continue;

                                    if (ctx->isAdmin) {
                                        // Admins always get full presence
                                        conn->send(fullMsg);
                                    } else if (visibility == "approx_only") {
                                        // Non-admins get approximate info
                                        conn->send(approxMsg);
                                    }
                                    // visibility == "nobody": non-admins get nothing
                                } catch (const std::exception& e) {
                                    LOG_ERROR << "WS presence send error: " << e.what();
                                }
                            }
                        }
                    },
                    [userId](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "broadcastPresence DB error for user " << userId
                                  << ": " << e.base().what();
                    },
                    userId);
            }
        },
        [userId](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "broadcastPresence settings query error for user " << userId
                      << ": " << e.base().what();
        },
        userId);
}

// ── WebSocket lifecycle ────────────────────────────────────────────────────

void WsHandler::handleNewConnection(const drogon::HttpRequestPtr& req,
                                    const drogon::WebSocketConnectionPtr& conn) {
    try {
        MetricsService::instance().wsConnect();
        auto ctx = std::make_shared<ConnCtx>();
        conn->setContext(ctx);
        LOG_INFO << "WebSocket opened from " << conn->peerAddr().toIpPort();
    } catch (const std::exception& e) {
        LOG_ERROR << "WS handleNewConnection error: " << e.what();
        try { conn->forceClose(); } catch (...) {}
    } catch (...) {
        LOG_ERROR << "WS handleNewConnection unknown error";
        try { conn->forceClose(); } catch (...) {}
    }
}

void WsHandler::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    try {
        MetricsService::instance().wsDisconnect();
        auto ctx = conn->getContext<ConnCtx>();
        if (ctx) {
            {
                std::lock_guard<std::mutex> lk(s_mu);
                for (long long chatId : ctx->subscriptions) {
                    auto& vec = s_subs[chatId];
                    vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
                }
            }

            // Remove from user connections and broadcast offline if last
            if (ctx->authed && ctx->userId > 0) {
                bool nowOffline = false;
                {
                    std::lock_guard<std::mutex> lk(s_userMu);
                    auto it = s_userConns.find(ctx->userId);
                    if (it != s_userConns.end()) {
                        auto& vec = it->second;
                        vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());
                        nowOffline = vec.empty();
                        if (nowOffline) s_userConns.erase(it);
                    }
                }
                if (nowOffline) {
                    broadcastPresence(ctx->userId, ctx->username, "offline");
                    // Update last_activity
                    auto db = drogon::app().getDbClient();
                    db->execSqlAsync(
                        "UPDATE users SET last_activity = NOW() WHERE id = $1",
                        [](const drogon::orm::Result&) {},
                        [](const drogon::orm::DrogonDbException&) {},
                        ctx->userId);
                }
            }
        }
        LOG_INFO << "WebSocket closed";
    } catch (const std::exception& e) {
        LOG_ERROR << "WS handleConnectionClosed error: " << e.what();
    } catch (...) {
        LOG_ERROR << "WS handleConnectionClosed unknown error";
    }
}

void WsHandler::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                  std::string&&                         rawMsg,
                                  const drogon::WebSocketMessageType&   msgType) {
    try {
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

        // ── auth ───────────────────────────────────────────────────────────
        if (type == "auth") {
            std::string token = msg["token"].asString();
            auto claims = JwtService::instance().verify(token);
            if (!claims || claims->tokenType != "access") {
                sendError(conn, "Invalid or expired access token");
                conn->forceClose();
                return;
            }
            ctx->userId  = claims->userId;
            ctx->authed  = true;
            ctx->isAdmin = claims->isAdmin;

            // Update last_activity and fetch username for typing/presence
            auto db = drogon::app().getDbClient();
            db->execSqlAsync(
                "UPDATE users SET last_activity = NOW() WHERE id = $1 RETURNING username",
                [this, ctx, conn](const drogon::orm::Result& r) {
                    if (!r.empty()) ctx->username = r[0]["username"].as<std::string>();
                    // Track user connection for presence
                    bool wasOffline = false;
                    {
                        std::lock_guard<std::mutex> lk(s_userMu);
                        auto& vec = s_userConns[ctx->userId];
                        wasOffline = vec.empty();
                        vec.push_back(conn);
                    }
                    if (wasOffline) {
                        broadcastPresence(ctx->userId, ctx->username, "online");
                    }
                },
                [](const drogon::orm::DrogonDbException&) {},
                ctx->userId);

            Json::Value ok;
            ok["type"]    = "auth_ok";
            ok["user_id"] = Json::Int64(ctx->userId);
            sendJson(conn, ok);
            return;
        }

        if (!ctx->authed) { sendError(conn, "Not authenticated"); return; }

        // ── ping ───────────────────────────────────────────────────────────
        if (type == "ping") {
            Json::Value pong; pong["type"] = "pong";
            sendJson(conn, pong);
            return;
        }

        // ── subscribe ──────────────────────────────────────────────────────
        if (type == "subscribe") {
            long long chatId = msg["chat_id"].asInt64();
            if (chatId <= 0) { sendError(conn, "Invalid chat_id"); return; }

            long long userId = ctx->userId;
            auto db = drogon::app().getDbClient();
            db->execSqlAsync(
                "SELECT 1 FROM chat_members WHERE chat_id = $1 AND user_id = $2",
                [this, conn, ctx, chatId](const drogon::orm::Result& r) {
                    try {
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
                    } catch (const std::exception& e) {
                        LOG_ERROR << "WS subscribe callback error: " << e.what();
                    }
                },
                [conn](const drogon::orm::DrogonDbException& e) {
                    if (conn->disconnected()) return;
                    LOG_ERROR << "WS subscribe membership check: " << e.base().what();
                    sendError(conn, "Internal error");
                },
                chatId, userId);
            return;
        }

        // ── typing ──────────────────────────────────────────────────────
        if (type == "typing") {
            long long chatId = msg["chat_id"].asInt64();
            if (chatId <= 0) { sendError(conn, "Invalid chat_id"); return; }

            // Build typing payload with sender info
            Json::Value payload;
            payload["type"]    = "typing";
            payload["chat_id"] = Json::Int64(chatId);
            payload["user_id"] = Json::Int64(ctx->userId);
            if (!ctx->username.empty())
                payload["username"] = ctx->username;

            // Publish via Redis so all nodes get it
            WsDispatch::publishMessage(chatId, payload);
            return;
        }

        sendError(conn, "Unknown message type: " + type);
    } catch (const std::exception& e) {
        LOG_ERROR << "WS handleNewMessage error: " << e.what();
        try { conn->forceClose(); } catch (...) {}
    } catch (...) {
        LOG_ERROR << "WS handleNewMessage unknown error";
        try { conn->forceClose(); } catch (...) {}
    }
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
