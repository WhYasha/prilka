#pragma once
#include <drogon/HttpController.h>

class UsersController : public drogon::HttpController<UsersController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UsersController::getUser,           "/users/{1}",           drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(UsersController::searchUsers,       "/users/search",        drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(UsersController::getUserByUsername, "/users/by-username/{1}", drogon::Get);
    ADD_METHOD_TO(UsersController::getUserAvatar,     "/users/{1}/avatar",    drogon::Get,  "AuthFilter");
    ADD_METHOD_TO(UsersController::updateUser,        "/users/{1}",           drogon::Put,  "AuthFilter");
    ADD_METHOD_TO(UsersController::updateMyAvatar,    "/users/me/avatar",     drogon::Put,  "AuthFilter");
    METHOD_LIST_END

    void getUser(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long long userId);

    void searchUsers(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void getUserByUsername(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                           const std::string& username);

    void getUserAvatar(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       long long userId);

    void updateUser(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long long userId);

    void updateMyAvatar(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
