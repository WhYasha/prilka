#pragma once
#include <drogon/HttpController.h>

class AdminMessagesController : public drogon::HttpController<AdminMessagesController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminMessagesController::listMessages, "/admin-api/messages", drogon::Get, "AdminFilter");
    METHOD_LIST_END

    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
