#pragma once
#include <string>

// We avoid "using namespace std" in header files to prevent naming collisions!
struct Metric {
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