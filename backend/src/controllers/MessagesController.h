#pragma once
#include <drogon/HttpController.h>

class MessagesController : public drogon::HttpController<MessagesController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MessagesController::sendMessage,  "/chats/{1}/messages", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(MessagesController::listMessages, "/chats/{1}/messages", drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(MessagesController::deleteMessage, "/chats/{1}/messages/{2}", drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(MessagesController::editMessage, "/chats/{1}/messages/{2}", drogon::Put, "AuthFilter");
    ADD_METHOD_TO(MessagesController::forwardMessages, "/chats/{1}/forward", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(MessagesController::pinMessage,      "/chats/{1}/messages/{2}/pin", drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(MessagesController::unpinMessage,     "/chats/{1}/messages/{2}/pin", drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(MessagesController::getPinnedMessage, "/chats/{1}/pinned-message",   drogon::Get,    "AuthFilter");
    METHOD_LIST_END

    void sendMessage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId);

    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long chatId);

    void deleteMessage(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       long long chatId, long long messageId);

    void editMessage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId, long long messageId);

    void forwardMessages(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                         long long targetChatId);

    void pinMessage(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long long chatId, long long messageId);

    void unpinMessage(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long chatId, long long messageId);

    void getPinnedMessage(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          long long chatId);
};
