#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
#include <unordered_map>
#include <mutex>
#include <string>

/// WebSocket endpoint: /ws
/// Protocol:
///   Client → Server:
///     { "type": "auth",      "token": "<access-jwt>" }
///     { "type": "subscribe", "chat_id": 42 }
///     { "type": "typing",    "chat_id": 42 }
///     { "type": "ping" }
///
///   Server → Client:
///     { "type": "pong" }
///     { "type": "error", "message": "..." }
///     { "type": "message", "chat_id": 42, "sender_id": 7, "content": "hi", "id": 99, "created_at": "..." }
///     { "type": "typing",  "chat_id": 42, "user_id": 7, "username": "alice" }
///     { "type": "presence", "user_id": 7, "status": "online" }
///     { "type": "reaction", "chat_id": 42, "message_id": 99, "user_id": 7, "emoji": "...", "action": "added|removed" }
///     { "type": "message_deleted", "chat_id": 42, "message_id": 99, "deleted_by": 7, "for_everyone": true }
///
/// Fan-out uses Redis Pub/Sub channel "chat:<chat_id>"
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

private:
    // Per-connection state stored in conn->getContext()
    struct ConnCtx {
        long long userId   = 0;
        bool      authed   = false;
        std::string username;
        std::vector<long long> subscriptions;
    };

    // Subscribe this process to Redis channel "chat:<chatId>" if not already done.
    void subscribeToRedis(long long chatId);

    // Broadcast presence (online/offline) to all chats the user belongs to.
    void broadcastPresence(long long userId, const std::string& username,
                           const std::string& status);

    // Map: chatId → list of local connections subscribed
    static std::mutex                                              s_mu;
    static std::unordered_map<long long,
           std::vector<drogon::WebSocketConnectionPtr>>           s_subs;
    // Set of Redis channels already subscribed
    static std::unordered_map<long long, bool>                    s_redisSubs;

    // Per-user connection tracking for presence
    static std::mutex                                              s_userMu;
    static std::unordered_map<long long,
           std::vector<drogon::WebSocketConnectionPtr>>           s_userConns;
};

// Called by MessagesController after a message is persisted.
// Publishes to Redis so all nodes fan-out to local WS subscribers.
namespace WsDispatch {
    void publishMessage(long long chatId, const Json::Value& payload);
}
