#pragma once
#include <drogon/HttpFilter.h>

/// JWT authentication filter.
/// Add "AuthFilter" to any route that requires a logged-in user.
/// Sets attribute "user_id" (long long) on the request on success.
class AuthFilter : public drogon::HttpFilter<AuthFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&&       fcb,
                  drogon::FilterChainCallback&&  fccb) override;
};
