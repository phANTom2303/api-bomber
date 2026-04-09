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

    Result finalResult = formulateResults(responseTimes);

    displayResults(finalResult);
    exportResultsToJson(responseTimes, finalResult);

    curl_global_cleanup();
    return 0;
}