#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
#include <unordered_map>
#include <mutex>
#include <string>

/// WebSocket endpoint: /ws
/// Protocol:
///   Client → Server:
///     { "type": "auth",      "token": "<access-jwt>", "active": true|false }
///     { "type": "subscribe", "chat_id": 42 }
///     { "type": "typing",    "chat_id": 42 }
///     { "type": "presence_update", "status": "active"|"away" }
///     { "type": "ping" }                                   — keepalive only
///     { "type": "ping", "active": true }                   — activity-based presence refresh
///
///   Server → Client:
///     { "type": "pong" }
///     { "type": "error", "message": "..." }
///     { "type": "message", "chat_id": 42, "sender_id": 7, "content": "hi", "id": 99, "created_at": "...", "reply_to_message_id": 50 }
///     { "type": "typing",  "chat_id": 42, "user_id": 7, "username": "alice" }
///     { "type": "presence", "user_id": 7, "status": "online" }
///     { "type": "reaction", "chat_id": 42, "message_id": 99, "user_id": 7, "emoji": "...", "action": "added|removed" }
///     { "type": "message_deleted", "chat_id": 42, "message_id": 99, "deleted_by": 7, "for_everyone": true }
///     { "type": "message_updated", "chat_id": 42, "message_id": 99, "content": "edited text", "updated_at": "..." }
///     { "type": "message_pinned", "chat_id": 42, "message_id": 99, "pinned_by": 7, "message": {...} }
///     { "type": "message_unpinned", "chat_id": 42, "message_id": 99 }
///     { "type": "read_receipt", "chat_id": 42, "user_id": 7, "last_read_msg_id": 500 }
///     { "type": "chat_created", "chat_id": 42, "chat_type": "group", "title": "...", "created_by": 1 }
///     { "type": "chat_updated", "chat_id": 42, "title?": "...", "description?": "...", "avatar_url?": "..." }
///     { "type": "chat_deleted", "chat_id": 42, "deleted_by": 1 }
///     { "type": "chat_member_joined", "chat_id": 42, "user_id": 7, "username": "alice", "display_name": "Alice" }
///     { "type": "chat_member_left", "chat_id": 42, "user_id": 7 }
///     { "type": "user_profile_updated", "user_id": 7, "display_name?": "...", "avatar_url?": "..." }
///
/// Fan-out uses Redis Pub/Sub:
///   - "chat:<chat_id>" for chat-scoped events (messages, typing, reactions, etc.)
///   - "user:<user_id>" for user-scoped events (chat_created, chat_deleted, profile updates)
class WsHandler : public drogon::WebSocketController<WsHandler> {
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws");
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&&                         message,
                          const drogon::WebSocketMessageType&   type) override;

    void handleNewConnection(const drogon::HttpRequestPtr&       req,
                             const drogon::WebSocketConnectionPtr& conn) override;

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    // Push a JSON message to all connections subscribed to a chat (local fan-out).
    static void broadcast(long long chatId, const Json::Value& payload);

    // Push a JSON message to all connections of a specific user (local fan-out).
    static void broadcastToUser(long long userId, const Json::Value& payload);

    // Check if a user has any active WebSocket connections.
    static bool isUserOnline(long long userId);

private:
    // Per-connection state stored in conn->getContext()
    struct ConnCtx {
        long long userId   = 0;
        bool      authed   = false;
        bool      isAdmin  = false;
        bool      active   = true;   // Whether this connection's tab/window is visible+focused
        std::string username;
        std::vector<long long> subscriptions;
        std::chrono::steady_clock::time_point lastDbRefresh;  // Throttle DB last_activity updates
    };

    // Subscribe this process to Redis channel "chat:<chatId>" if not already done.
    void subscribeToRedis(long long chatId);

    // Subscribe this process to Redis channel "user:<userId>" if not already done.
    void subscribeToUserRedis(long long userId);

    // Broadcast presence (online/offline) to all chats the user belongs to.
    void broadcastPresence(long long userId, const std::string& username,
                           const std::string& status);

    // Map: chatId → list of local connections subscribed
    static std::mutex                                              s_mu;
    static std::unordered_map<long long,
           std::vector<drogon::WebSocketConnectionPtr>>           s_subs;
    // Set of Redis chat channels already subscribed
    static std::unordered_map<long long, bool>                    s_redisSubs;
    // Set of Redis user channels already subscribed
    static std::unordered_map<long long, bool>                    s_redisUserSubs;

    // Per-user connection tracking for presence
    static std::mutex                                              s_userMu;
    static std::unordered_map<long long,
           std::vector<drogon::WebSocketConnectionPtr>>           s_userConns;
};

// Called by controllers after mutations are persisted.
// Publishes to Redis so all nodes fan-out to local WS subscribers.
namespace WsDispatch {
    // Publish to chat:<chatId> channel (messages, typing, reactions, chat_updated, etc.)
    void publishMessage(long long chatId, const Json::Value& payload);
    // Publish to user:<userId> channel (chat_created, chat_deleted, profile updates)
    void publishToUser(long long userId, const Json::Value& payload);
}
