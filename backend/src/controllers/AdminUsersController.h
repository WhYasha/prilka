#pragma once
#include <drogon/HttpController.h>

class AdminUsersController : public drogon::HttpController<AdminUsersController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminUsersController::listUsers,    "/admin-api/users",                  drogon::Get,  "AdminFilter");
    ADD_METHOD_TO(AdminUsersController::getUser,      "/admin-api/users/{1}",              drogon::Get,  "AdminFilter");
    ADD_METHOD_TO(AdminUsersController::blockUser,    "/admin-api/users/{1}/block",        drogon::Post, "AdminFilter");
    ADD_METHOD_TO(AdminUsersController::unblockUser,  "/admin-api/users/{1}/unblock",      drogon::Post, "AdminFilter");
    ADD_METHOD_TO(AdminUsersController::softDelete,   "/admin-api/users/{1}/soft-delete",  drogon::Post, "AdminFilter");
    ADD_METHOD_TO(AdminUsersController::toggleAdmin,  "/admin-api/users/{1}/toggle-admin", drogon::Post, "AdminFilter");
    METHOD_LIST_END

    void listUsers(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void getUser(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long long userId);

    void blockUser(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                   long long userId);

    void unblockUser(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long userId);

    void softDelete(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long long userId);

    void toggleAdmin(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     long long userId);
};
