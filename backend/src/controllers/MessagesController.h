#pragma once
#include <drogon/HttpController.h>

class MessagesController : public drogon::HttpController<MessagesController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MessagesController::sendMessage,  "/chats/{1}/messages", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(MessagesController::listMessages, "/chats/{1}/messages", drogon::Get,  "AuthFilter");
    METHOD_LIST_END

    void sendMessage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId);

    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long chatId);
};
