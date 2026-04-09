#include "Metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <json.hpp>

using namespace std;

long long getTimeStamp() {
    auto now = chrono::system_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch())
        .count();
}

Result formulateResults(vector<Metric>& responseTimes) {
    // 5 vectors, one for each of :
    //  1. Total time
    //  2. Server Latency
    //  3. DNS Lookup Time
    //  4. TCP Handshake Time
    //  5. TLS resolution Time
    vector<vector<double>> separateMetricData(5, vector<double>());
    vector<long double> aggregates(5, 0.0);

    int successCount = 0;
    for (int i = 0; i < responseTimes.size(); i++) {
        if (responseTimes[i].total_time < 0)  // skip failed requests
            continue;

        auto& [response_code, timeStamp, total_time, server_latency, dns_time,
               tcp_time, tls_time] = responseTimes[i];

        if (response_code >= 200 && response_code < 300)
            successCount++;

        vector<double> temp = {total_time, server_latency, dns_time, tcp_time,
                               tls_time};

        for (int j = 0; j < 5; j++) {
            separateMetricData[j].push_back(temp[j]);
            aggregates[j] += temp[j];
        }
    }

    vector<double> averagaes(5, 0.0);
    for (int i = 0; i < 5; i++) {
        averagaes[i] = (aggregates[i] / successCount);
        sort(separateMetricData[i].begin(), separateMetricData[i].end());
    }

    int p95_index = (int)ceil(0.95 * (double)successCount) - 1;
    int p99_index = (int)ceil(0.99 * (double)successCount) - 1;

    vector<vector<double>> results(5, vector<double>(3, 0.0));

    for (int i = 0; i < 5; i++) {
        results[i][0] = averagaes[i];
        results[i][1] = separateMetricData[i][p95_index];
        results[i][2] = separateMetricData[i][p99_index];
    }

    Result res;
    res.metricStatistics = results;
    res.successCount = successCount;
    res.failCount = responseTimes.size() - successCount;
    return res;
}

void displayResults(Result res) {
    cout << "Successful requests : " << res.successCount
         << " \n Failed requests : " << res.failCount << " \n";
    vector<vector<double>>& results = res.metricStatistics;
    vector<string> metricNames = {"Total Time", "Server Latency",
                                  "DNS Lookup Time", "TCP Handshake Time",
                                  "TLS resolution Time"};

    for (int i = 0; i < 5; i++) {
        cout << "\nMetric : " << metricNames[i] << " \n";
        cout << "Average : " << results[i][0] << " ms | P95 : " << results[i][1]
             << " ms | P99 : " << results[i][2] << " ms\n";
    }
}

void exportResultsToJson(const vector<Metric>& rawData, Result res) {
    // 1. Auto-generate the filename using your existing timestamp function
    long long timestamp = getTimeStamp();
    string filename = "bomber-run-" + to_string(timestamp) + ".json";

    // Initialize the main JSON object
    nlohmann::ordered_json outputData;
    outputData["summary"]["succes_count"] = res.successCount;
    outputData["summary"]["failed_count"] = res.failCount;
    // 2. Populate the Summary Statistics
    vector<string> metricNames = {"Total Time", "Server Latency",
                                  "DNS Lookup Time", "TCP Handshake Time",
                                  "TLS Resolution Time"};
    vector<vector<double>>& summaryResults = res.metricStatistics;
    for (int i = 0; i < 5; i++) {
        outputData["summary"][metricNames[i]] = {
            {"average_ms", summaryResults[i][0]},
            {"p95_ms", summaryResults[i][1]},
            {"p99_ms", summaryResults[i][2]}};
    }

        // 3. Populate the Raw Data Array
    outputData["raw_data"] = nlohmann::json::array();

    for (const auto& metric : rawData) {
        nlohmann::ordered_json metricObj = {
            {"timestamp", metric.timeStamp},
            {"total_time_ms", metric.total_time},
            {"server_latency_ms", metric.server_latency},
            {"dns_time_ms", metric.dns_time},
            {"tcp_time_ms", metric.tcp_time},
            {"tls_time_ms", metric.tls_time}};
        outputData["raw_data"].push_back(metricObj);
    }

    // 4. Write to the dynamically named file
    ofstream outputFile(filename);
    if (outputFile.is_open()) {
        outputFile << outputData.dump(4);
        outputFile.close();
        cout << "\n[SUCCESS] Results successfully exported to " << filename
             << "\n";
    } else {
        cerr << "\n[ERROR] Failed to open file for writing: " << filename
             << "\n";
    }
}
