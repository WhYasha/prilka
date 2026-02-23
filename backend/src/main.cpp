/**
 * Messenger C++ Backend — main entry point
 * Framework: Drogon (https://github.com/drogonframework/drogon)
 *
 * Endpoints:
 *   POST /auth/register       POST /auth/login    POST /auth/refresh
 *   GET  /me
 *   GET  /users/{id}          GET  /users/search
 *   POST /chats               GET  /chats          GET  /chats/{id}
 *   POST /chats/{id}/messages GET  /chats/{id}/messages
 *   POST /files               GET  /files/{id}/download
 *   WS   /ws
 *   GET  /health              GET  /metrics
 */

#include <drogon/drogon.h>
#include "config/Config.h"
#include "services/MetricsService.h"
#include "controllers/AuthController.h"
#include "controllers/UsersController.h"
#include "controllers/ChatsController.h"
#include "controllers/MessagesController.h"
#include "controllers/FilesController.h"
#include "filters/AuthFilter.h"
#include "ws/WsHandler.h"
#include <trantor/utils/Logger.h>
#include <csignal>
#include <iostream>
#include <chrono>

static void onSignal(int) {
    LOG_INFO << "Shutting down…";
    drogon::app().quit();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // ── Load config ──────────────────────────────────────────────────────────
    Config cfg;
    try {
        cfg = Config::fromEnv();
    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] Config error: " << ex.what() << "\n";
        return 1;
    }

    LOG_INFO << "Starting Messenger API on port " << cfg.apiPort;

    // ── PostgreSQL connection pool ────────────────────────────────────────────
    drogon::app().createDbClient(
        "postgresql",
        cfg.dbHost,
        static_cast<unsigned short>(cfg.dbPort),
        cfg.dbName,
        cfg.dbUser,
        cfg.dbPass,
        /*connectionNum=*/ 10,
        /*filename=*/ "",
        /*name=*/ "default",
        /*isFast=*/ false,
        /*characterSet=*/ "utf8",
        /*timeout=*/ 30.0
    );

    // ── Redis client ──────────────────────────────────────────────────────────
    if (!cfg.redisHost.empty()) {
        drogon::app().createRedisClient(
            cfg.redisHost,
            static_cast<unsigned short>(cfg.redisPort),
            "default",       // connection name
            cfg.redisPass,
            /*connectionNum=*/ 4
        );
        LOG_INFO << "Redis configured at " << cfg.redisHost << ":" << cfg.redisPort;
    }

    // ── Health endpoint ───────────────────────────────────────────────────────
    drogon::app().registerHandler(
        "/health",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            Json::Value body;
            body["status"] = "ok";
            body["service"] = "messenger-api-cpp";
            cb(drogon::HttpResponse::newHttpJsonResponse(body));
        },
        {drogon::Get});

    // ── Prometheus metrics endpoint ────────────────────────────────────────────
    drogon::app().registerHandler(
        "/metrics",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(MetricsService::instance().expose());
            resp->setContentTypeString("text/plain; version=0.0.4; charset=utf-8");
            cb(resp);
        },
        {drogon::Get});

    // ── Metrics middleware (wraps all /api/* routes) ───────────────────────────
    // For MVP: metrics are recorded per-controller. A global middleware approach
    // can be added via drogon::app().registerPreHandlingAdvice() in Drogon 1.9+.
    // TODO: add a global advice that records latency + status for every request.

    // ── Server configuration ──────────────────────────────────────────────────
    drogon::app()
        .addListener("0.0.0.0", cfg.apiPort)
        .setThreadNum(cfg.apiThreads)      // 0 = std::thread::hardware_concurrency()
        .setMaxConnectionNum(100000)
        .setMaxConnectionNumPerIP(1000)
        .setIdleConnectionTimeout(60)
        .setLogLevel(trantor::Logger::LogLevel::kInfo)
        .enableServerHeader(false)          // don't leak framework info
        .setClientMaxBodySize(
            static_cast<size_t>(Config::get().maxFileSizeMb) * 1024 * 1024 + 1024)
        .run();

    return 0;
}
