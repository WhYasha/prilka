#include "AuthFilter.h"
#include "../services/JwtService.h"
#include <drogon/HttpResponse.h>

void AuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                           drogon::FilterCallback&&      fcb,
                           drogon::FilterChainCallback&& fccb) {
    std::string auth = req->getHeader("Authorization");

    auto reject = [&](const std::string& msg) {
        Json::Value body;
        body["error"] = msg;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
    };

    if (auth.empty()) {
        return reject("Missing Authorization header");
    }
    if (auth.substr(0, 7) != "Bearer ") {
        return reject("Authorization header must start with 'Bearer '");
    }

    std::string token = auth.substr(7);
    auto claims = JwtService::instance().verify(token);
    if (!claims) {
        return reject("Invalid or expired token");
    }
    if (claims->tokenType != "access") {
        return reject("Expected access token");
    }

    req->getAttributes()->insert("user_id", claims->userId);
    fccb();  // pass through
}
