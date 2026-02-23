#include <gtest/gtest.h>
#include "services/MetricsService.h"
#include <string>

TEST(MetricsService, RecordsRequests) {
    auto& m = MetricsService::instance();
    m.incRequest("GET", "/health", 200);
    m.incRequest("POST", "/auth/login", 401);

    std::string exposed = m.expose();
    EXPECT_NE(exposed.find("messenger_http_requests_total"), std::string::npos);
    EXPECT_NE(exposed.find("GET"), std::string::npos);
    EXPECT_NE(exposed.find("200"), std::string::npos);
}

TEST(MetricsService, RecordsLatency) {
    auto& m = MetricsService::instance();
    m.observeLatency("GET", "/health", 0.005);
    m.observeLatency("GET", "/health", 0.1);

    std::string exposed = m.expose();
    EXPECT_NE(exposed.find("messenger_http_request_duration_seconds"), std::string::npos);
    EXPECT_NE(exposed.find("_sum"), std::string::npos);
    EXPECT_NE(exposed.find("_count"), std::string::npos);
}

TEST(MetricsService, WebSocketGauge) {
    auto& m = MetricsService::instance();
    m.wsConnect();
    m.wsConnect();

    std::string exposed = m.expose();
    EXPECT_NE(exposed.find("messenger_ws_connections_active"), std::string::npos);
    EXPECT_NE(exposed.find("messenger_ws_connections_total"), std::string::npos);

    m.wsDisconnect();
    m.wsDisconnect();
}
