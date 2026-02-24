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
    ADD_METHOD_TO(ChatsController::markRead,      "/chats/{1}/read",     drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(ChatsController::pinChat,        "/chats/{1}/pin",      drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(ChatsController::unpinChat,      "/chats/{1}/pin",      drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(ChatsController::archiveChat,    "/chats/{1}/archive",  drogon::Post,   "AuthFilter");
    ADD_METHOD_TO(ChatsController::unarchiveChat,  "/chats/{1}/archive",  drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(ChatsController::updateChat,     "/chats/{1}",          drogon::Patch,  "AuthFilter");
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

    void markRead(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  long long chatId);

    void pinChat(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long long chatId);

    void unpinChat(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                   long long chatId);

    void archiveChat(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId);

    void unarchiveChat(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       long long chatId);

    void updateChat(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long long chatId);
};
