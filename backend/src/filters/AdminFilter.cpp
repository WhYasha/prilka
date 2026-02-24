#include "AdminFilter.h"
#include "../services/JwtService.h"
#include <drogon/HttpResponse.h>

void AdminFilter::doFilter(const drogon::HttpRequestPtr& req,
                            drogon::FilterCallback&&      fcb,
                            drogon::FilterChainCallback&& fccb) {
    std::string auth = req->getHeader("Authorization");

    auto reject = [&](const std::string& msg, drogon::HttpStatusCode code) {
        Json::Value body;
        body["error"] = msg;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(code);
        fcb(resp);
    };

    if (auth.empty()) {
        return reject("Missing Authorization header", drogon::k401Unauthorized);
    }
    if (auth.substr(0, 7) != "Bearer ") {
        return reject("Authorization header must start with 'Bearer '", drogon::k401Unauthorized);
    }

    std::string token = auth.substr(7);
    auto claims = JwtService::instance().verify(token);
    if (!claims) {
        return reject("Invalid or expired token", drogon::k401Unauthorized);
    }
    if (claims->tokenType != "access") {
        return reject("Expected access token", drogon::k401Unauthorized);
    }
    if (!claims->isAdmin) {
        return reject("Admin access required", drogon::k403Forbidden);
    }

    req->getAttributes()->insert("user_id", claims->userId);
    fccb();  // pass through
}
