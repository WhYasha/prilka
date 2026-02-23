#pragma once
#include <drogon/HttpController.h>

class ChatsController : public drogon::HttpController<ChatsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ChatsController::createChat, "/chats",     drogon::Post, "AuthFilter");
    ADD_METHOD_TO(ChatsController::listChats,  "/chats",     drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(ChatsController::getChat,    "/chats/{1}", drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(ChatsController::getChatByPublicName, "/chats/by-name/{1}", drogon::Get);
    METHOD_LIST_END

    void createChat(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void listChats(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void getChat(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long long chatId);

    void getChatByPublicName(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                             const std::string& publicName);
};
