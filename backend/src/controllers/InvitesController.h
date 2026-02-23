#pragma once
#include <drogon/HttpController.h>

class InvitesController : public drogon::HttpController<InvitesController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(InvitesController::createInvite, "/chats/{1}/invites", drogon::Post, "AuthFilter");
    ADD_METHOD_TO(InvitesController::listInvites,  "/chats/{1}/invites", drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(InvitesController::revokeInvite,  "/invites/{1}",         drogon::Delete, "AuthFilter");
    ADD_METHOD_TO(InvitesController::previewInvite, "/invites/{1}/preview", drogon::Get);
    ADD_METHOD_TO(InvitesController::joinInvite,    "/invites/{1}/join",    drogon::Post, "AuthFilter");
    METHOD_LIST_END

    void createInvite(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      long long chatId);

    void listInvites(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long chatId);

    void revokeInvite(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      const std::string& token);

    void previewInvite(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       const std::string& token);

    void joinInvite(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    const std::string& token);
};
