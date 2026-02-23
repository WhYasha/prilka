#pragma once
#include <string>
#include <cstdlib>
#include <stdexcept>

/// All runtime configuration read from environment variables.
struct Config {
    // PostgreSQL
    std::string dbHost;
    int         dbPort;
    std::string dbName;
    std::string dbUser;
    std::string dbPass;

    // Redis
    std::string redisHost;
    int         redisPort;
    std::string redisPass;

    // MinIO
    std::string minioEndpoint;
    std::string minioAccessKey;
    std::string minioSecretKey;
    std::string minioBucket;
    bool        minioUseSSL;

    // JWT
    std::string jwtSecret;
    int         jwtAccessTtl;   // seconds
    int         jwtRefreshTtl;  // seconds

    // Server
    int  apiPort;
    int  apiThreads;
    long maxFileSizeMb;

    // ----------------------------------------------------------------
    static Config fromEnv() {
        Config c;
        c.dbHost        = getenv_or("DB_HOST",          "localhost");
        c.dbPort        = getenv_int("DB_PORT",          5432);
        c.dbName        = getenv_or("DB_NAME",          "messenger");
        c.dbUser        = getenv_or("DB_USER",          "messenger");
        c.dbPass        = getenv_or("DB_PASS",          "changeme_postgres");

        c.redisHost     = getenv_or("REDIS_HOST",       "localhost");
        c.redisPort     = getenv_int("REDIS_PORT",       6379);
        c.redisPass     = getenv_or("REDIS_PASS",       "");

        c.minioEndpoint = getenv_or("MINIO_ENDPOINT",   "localhost:9000");
        c.minioAccessKey= getenv_or("MINIO_ACCESS_KEY", "minioadmin");
        c.minioSecretKey= getenv_or("MINIO_SECRET_KEY", "changeme_minio");
        c.minioBucket   = getenv_or("MINIO_BUCKET",     "messenger-files");
        c.minioUseSSL   = false;

        c.jwtSecret     = getenv_or("JWT_SECRET",       "change-me");
        c.jwtAccessTtl  = getenv_int("JWT_ACCESS_TTL",  3600);
        c.jwtRefreshTtl = getenv_int("JWT_REFRESH_TTL", 604800);

        c.apiPort       = getenv_int("API_PORT",         8080);
        c.apiThreads    = getenv_int("API_THREADS",      0);
        c.maxFileSizeMb = getenv_int("MAX_FILE_SIZE_MB", 50);

        if (c.jwtSecret == "change-me" || c.jwtSecret.size() < 16) {
            throw std::runtime_error("JWT_SECRET is not set or too short (min 16 chars)");
        }
        return c;
    }

    static const Config& get() {
        static Config instance = fromEnv();
        return instance;
    }

private:
    static std::string getenv_or(const char* name, const char* def) {
        const char* v = std::getenv(name);
        return v ? std::string(v) : std::string(def);
    }
    static int getenv_int(const char* name, int def) {
        const char* v = std::getenv(name);
        return v ? std::atoi(v) : def;
    }
};
