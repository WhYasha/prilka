#include "WebController.h"
#include <trantor/utils/Logger.h>
#include <fstream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static drogon::HttpResponsePtr serveHtml(const std::string& path) {
    std::string content = readFile(path);
    if (content.empty()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k404NotFound);
        resp->setBody("404 Not Found");
        return resp;
    }
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(content);
    resp->setContentTypeString("text/html; charset=utf-8");
    return resp;
}

void WebController::serveApp(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveDeepLink(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   const std::string& /*segment*/) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveLogin(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveRegister(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveMessageDeepLink(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                          const std::string& /*chatId*/,
                                          const std::string& /*messageId*/) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveAdmin(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveAdminSub(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   const std::string& /*sub1*/) {
    cb(serveHtml("./www/index.html"));
}

void WebController::serveAdminSubSub(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                      const std::string& /*sub1*/,
                                      const std::string& /*sub2*/) {
    cb(serveHtml("./www/index.html"));
}
