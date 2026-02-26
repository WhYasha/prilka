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

void WebController::serveDownload(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   const std::string& platform,
                                   const std::string& filename) {
    // Block path traversal
    if (platform.find("..") != std::string::npos ||
        filename.find("..") != std::string::npos) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        resp->setBody("403 Forbidden");
        cb(resp);
        return;
    }

    std::string path = "./www/downloads/" + platform + "/" + filename;
    auto resp = drogon::HttpResponse::newFileResponse(path);
    if (resp->statusCode() == drogon::k404NotFound) {
        cb(resp);
        return;
    }

    // Set Content-Disposition for binary downloads
    std::string ext;
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) ext = filename.substr(dot);

    if (ext == ".exe" || ext == ".msi" || ext == ".dmg" || ext == ".AppImage" ||
        ext == ".deb" || ext == ".rpm" || ext == ".zip" || ext == ".tar" || ext == ".gz") {
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    }

    cb(resp);
}
