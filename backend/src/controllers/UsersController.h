#pragma once
#include <drogon/HttpController.h>

class UsersController : public drogon::HttpController<UsersController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UsersController::getUser,   "/users/{1}", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(UsersController::searchUsers, "/users/search", drogon::Get, "AuthFilter");
    METHOD_LIST_END

    void getUser(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long long userId);

    void searchUsers(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
