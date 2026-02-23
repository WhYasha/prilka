#pragma once
#include <atomic>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

/// Thread-safe Prometheus text-format metrics registry.
/// Exposes a single /metrics endpoint without external library deps.
class MetricsService {
public:
    static MetricsService& instance();

    // Counter: total HTTP requests
    void incRequest(const std::string& method, const std::string& path, int status);

    // Histogram: record request duration in seconds
    void observeLatency(const std::string& method, const std::string& path,
                        double seconds);

    // Gauge: WebSocket connections
    void wsConnect();
    void wsDisconnect();

    // Render Prometheus text format
    std::string expose() const;

private:
    MetricsService() = default;

    mutable std::mutex mu_;

    struct Counter {
        std::atomic<long long> value{0};
    };

    // request_total[method][path][status]
    std::map<std::string, std::map<std::string, std::map<int, long long>>> reqTotal_;

    // latency histogram buckets (fixed)
    static constexpr double kBuckets[] = {0.005, 0.01, 0.025, 0.05, 0.1,
                                           0.25,  0.5,  1.0,   2.5,  5.0, 10.0};
    struct HistBucket {
        double sum{0.0};
        long long count{0};
        std::map<double, long long> buckets;
    };
    std::map<std::string, HistBucket> latHist_;  // key = method+path

    std::atomic<long long> wsActive_{0};
    std::atomic<long long> wsTotal_{0};
};
