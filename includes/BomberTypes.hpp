#pragma once
#include <string>
#include <vector>
// We avoid "using namespace std" in header files to prevent naming collisions!
struct Metric {
    long response_code;
    long long timeStamp;
    double total_time;
    double server_latency;
    double dns_time;
    double tcp_time;
    double tls_time;
};

struct Config {
    std::string url;
    int concurrent_requests;
    int requests_per_thread;
};

struct Result {
    int successCount;
    int failCount;
    std::vector<std::vector<double>> metricStatistics;
};