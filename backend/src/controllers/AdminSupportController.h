#pragma once
#include <drogon/HttpController.h>

class AdminSupportController : public drogon::HttpController<AdminSupportController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminSupportController::sendMessage,  "/admin-api/support/send",     drogon::Post, "AdminFilter");
    ADD_METHOD_TO(AdminSupportController::listChats,    "/admin-api/support/chats",    drogon::Get,  "AdminFilter");
    ADD_METHOD_TO(AdminSupportController::listMessages, "/admin-api/support/messages", drogon::Get,  "AdminFilter");
    ADD_METHOD_TO(AdminSupportController::listUsers,    "/admin-api/support/users",    drogon::Get,  "AdminFilter");
    METHOD_LIST_END

    void sendMessage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void listChats(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void listUsers(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
