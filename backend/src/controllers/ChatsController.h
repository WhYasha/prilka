#pragma once
#include <drogon/HttpController.h>

class ChatsController : public drogon::HttpController<ChatsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ChatsController::createChat, "/chats",     drogon::Post, "AuthFilter");
    ADD_METHOD_TO(ChatsController::listChats,  "/chats",     drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(ChatsController::getChat,    "/chats/{1}", drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(ChatsController::getChatByPublicName, "/chats/by-name/{1}", drogon::Get);
    ADD_METHOD_TO(ChatsController::addFavorite,    "/chats/{1}/favorite", drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(ChatsController::removeFavorite, "/chats/{1}/favorite", drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(ChatsController::muteChat,       "/chats/{1}/mute",     drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(ChatsController::unmuteChat,     "/chats/{1}/mute",     drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(ChatsController::leaveChat,      "/chats/{1}/leave",    drogon::Delete, "AuthFilter");
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

    void addFavorite(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId);

    void removeFavorite(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                        long long chatId);

    void muteChat(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  long long chatId);

    void unmuteChat(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long long chatId);

    void leaveChat(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                   long long chatId);
};
