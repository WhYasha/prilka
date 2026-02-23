#include "MetricsService.h"
#include <sstream>
#include <iomanip>

MetricsService& MetricsService::instance() {
    static MetricsService inst;
    return inst;
}

void MetricsService::incRequest(const std::string& method,
                                 const std::string& path, int status) {
    std::lock_guard<std::mutex> lk(mu_);
    reqTotal_[method][path][status]++;
}

void MetricsService::observeLatency(const std::string& method,
                                     const std::string& path, double seconds) {
    std::lock_guard<std::mutex> lk(mu_);
    std::string key = method + " " + path;
    auto& h = latHist_[key];
    h.sum += seconds;
    h.count++;
    for (double b : kBuckets) {
        if (seconds <= b) h.buckets[b]++;
    }
    h.buckets[std::numeric_limits<double>::infinity()]++;
}

void MetricsService::wsConnect()    { ++wsActive_; ++wsTotal_; }
void MetricsService::wsDisconnect() { if (wsActive_ > 0) --wsActive_; }

std::string MetricsService::expose() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::ostringstream out;

    // ── HTTP request counter ────────────────────────────────────────────────
    out << "# HELP messenger_http_requests_total Total HTTP requests\n"
        << "# TYPE messenger_http_requests_total counter\n";
    for (auto& [method, paths] : reqTotal_) {
        for (auto& [path, statuses] : paths) {
            for (auto& [status, count] : statuses) {
                out << "messenger_http_requests_total{"
                    << "method=\"" << method << "\","
                    << "path=\""   << path   << "\","
                    << "status=\"" << status << "\"} "
                    << count << "\n";
            }
        }
    }

    // ── Latency histogram ───────────────────────────────────────────────────
    out << "\n# HELP messenger_http_request_duration_seconds HTTP request latency\n"
        << "# TYPE messenger_http_request_duration_seconds histogram\n";
    for (auto& [key, h] : latHist_) {
        // Split key back into method/path
        auto sp = key.find(' ');
        std::string method = (sp != std::string::npos) ? key.substr(0, sp) : key;
        std::string path   = (sp != std::string::npos) ? key.substr(sp + 1) : "";
        std::string lbl = "method=\"" + method + "\",path=\"" + path + "\"";

        for (double b : kBuckets) {
            long long cnt = 0;
            auto it = h.buckets.find(b);
            if (it != h.buckets.end()) cnt = it->second;
            out << "messenger_http_request_duration_seconds_bucket{"
                << lbl << ",le=\"" << b << "\"} " << cnt << "\n";
        }
        // +Inf bucket
        out << "messenger_http_request_duration_seconds_bucket{"
            << lbl << ",le=\"+Inf\"} " << h.count << "\n";
        out << "messenger_http_request_duration_seconds_sum{"  << lbl << "} "
            << std::fixed << std::setprecision(6) << h.sum   << "\n";
        out << "messenger_http_request_duration_seconds_count{" << lbl << "} "
            << h.count << "\n\n";
    }

    // ── WebSocket gauge ──────────────────────────────────────────────────────
    out << "# HELP messenger_ws_connections_active Active WebSocket connections\n"
        << "# TYPE messenger_ws_connections_active gauge\n"
        << "messenger_ws_connections_active " << wsActive_.load() << "\n\n"
        << "# HELP messenger_ws_connections_total Total WebSocket connections opened\n"
        << "# TYPE messenger_ws_connections_total counter\n"
        << "messenger_ws_connections_total " << wsTotal_.load() << "\n";

    return out.str();
}

constexpr double MetricsService::kBuckets[];
