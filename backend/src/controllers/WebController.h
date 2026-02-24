#pragma once
#include <drogon/HttpController.h>

// WebController serves SPA HTML pages for all browser-navigable routes.
// Drogon's setDocumentRoot handles /static/ asset files.
class WebController : public drogon::HttpController<WebController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(WebController::serveApp,      "/app",      drogon::Get);
    ADD_METHOD_TO(WebController::serveLogin,    "/login",    drogon::Get);
    ADD_METHOD_TO(WebController::serveRegister, "/register", drogon::Get);
    // Deep-link routes — all serve index.html; JS reads window.location for the link
    ADD_METHOD_TO(WebController::serveDeepLink, "/@{1}",     drogon::Get);
    ADD_METHOD_TO(WebController::serveDeepLink, "/u/{1}",    drogon::Get);
    ADD_METHOD_TO(WebController::serveDeepLink, "/dm/{1}",   drogon::Get);
    ADD_METHOD_TO(WebController::serveDeepLink, "/join/{1}", drogon::Get);
    ADD_METHOD_TO(WebController::serveDeepLink, "/c/{1}",    drogon::Get);
    ADD_METHOD_TO(WebController::serveMessageDeepLink, "/c/{1}/{2}", drogon::Get);
    // Admin SPA (all sub-routes serve index.html for Vue Router)
    ADD_METHOD_TO(WebController::serveAdmin,         "/admin",          drogon::Get);
    ADD_METHOD_TO(WebController::serveAdminSub,      "/admin/{1}",      drogon::Get);
    ADD_METHOD_TO(WebController::serveAdminSubSub,   "/admin/{1}/{2}",  drogon::Get);
    METHOD_LIST_END

    void serveApp(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    // Deep-link handler: accepts the path parameter {1} and serves index.html
    void serveDeepLink(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       const std::string& segment);

    void serveLogin(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void serveRegister(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void serveAdmin(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void serveAdminSub(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       const std::string& sub1);

    // Message deep-link: /c/{chatId}/{messageId} — serves index.html
    void serveMessageDeepLink(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              const std::string& chatId,
                              const std::string& messageId);

    void serveAdminSubSub(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                          const std::string& sub1,
                          const std::string& sub2);
};
