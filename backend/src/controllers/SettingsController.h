#pragma once
#include <drogon/HttpController.h>

class SettingsController : public drogon::HttpController<SettingsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SettingsController::getSettings, "/settings", drogon::Get, "AuthFilter");
    ADD_METHOD_TO(SettingsController::putSettings, "/settings", drogon::Put, "AuthFilter");
    METHOD_LIST_END

    void getSettings(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void putSettings(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
