#pragma once
#include <drogon/HttpController.h>

class AdminDashboardController : public drogon::HttpController<AdminDashboardController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminDashboardController::getStats, "/admin-api/stats", drogon::Get, "AdminFilter");
    METHOD_LIST_END

    void getStats(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
