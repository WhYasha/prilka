#pragma once
#include <drogon/HttpFilter.h>

/// JWT authentication + admin check filter.
/// Add "AdminFilter" to any route that requires an admin user.
/// Sets attribute "user_id" (long long) on the request on success.
class AdminFilter : public drogon::HttpFilter<AdminFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&&       fcb,
                  drogon::FilterChainCallback&&  fccb) override;
};
