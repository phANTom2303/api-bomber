#include "BomberTypes.hpp"
#include "ConfigLoader.hpp"
#include "Engine.hpp"
#include "Metrics.hpp"
#include <curl/curl.h>

using namespace std;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Config config = loadConfig("bomber-config.json");

    vector<Metric> responseTimes = hammerURL(
        config.url, config.requests_per_thread, config.concurrent_requests);

    vector<vector<double>> results = formulateResults(responseTimes);

    displayResults(results);
    exportResultsToJson(responseTimes, results);

    curl_global_cleanup();
    return 0;
}