#include <curl/curl.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <mutex>
#include <string>
#include <thread>
using namespace std;

struct Metric {
    long long timeStamp;
    double total_time;
    double server_latency;
    double dns_time;
    double tcp_time;
    double tls_time;
};

struct Config {
    string url;
    int concurrent_requests;
    int requests_per_thread;
};

long long getTimeStamp() {
    // 1. Get the current time point
    auto now = std::chrono::system_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
    return ms;
}

// This callback function is called by libcurl as soon as there is data received
// that needs to be saved.
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    // Calculate the total size of the incoming chunk
    size_t totalSize = size * nmemb;

    // Cast the user pointer back to a std::string and append the data
    ((std::string*)userp)->append((char*)contents, totalSize);

    // Return the number of bytes processed so libcurl knows we handled it
    return totalSize;
}

Metric makeApiCall(const string& URL) {
    CURL* curl;
    CURLcode res;
    string readBuffer;  // This will hold the raw data

    // Initialize an easy session
    curl = curl_easy_init();
    Metric result;
    if (curl) {
        // 1. Set the URL you want to GET
        // Note : URL.c_str() is converting a cpp string object to c style
        // string i.e. char *
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

        // 2. Pass our custom callback function to handle incoming data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

        // 3. Pass the string variable to the callback function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // 4. Perform the request, res will get the return code
        result.timeStamp = getTimeStamp();
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                 << endl;
            result = {-1, -1, -1, -1, -1};
        } else {
            curl_off_t dns_time_us;
            curl_off_t tcp_time_us;
            curl_off_t tls_time_us;
            curl_off_t ttfb_us;
            curl_off_t total_time_us;

            // Fetch timings in microseconds
            curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &dns_time_us);
            curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &tcp_time_us);
            curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T, &tls_time_us);
            curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &ttfb_us);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total_time_us);

            result.dns_time = dns_time_us / 1000.0;
            result.tcp_time = (tcp_time_us - dns_time_us) / 1000.0;
            result.tls_time = (tls_time_us - tcp_time_us) / 1000.0;
            result.server_latency = (ttfb_us - tls_time_us) / 1000.0;
            result.total_time = total_time_us / 1000.0;
        }

        curl_easy_cleanup(curl);
    }

    return result;
}

vector<vector<double>> formulateResults(vector<Metric>& responseTimes) {
    // 5 vectors, one for each of :
    //  1. Total time
    //  2. Server Latency
    //  3. DNS Lookup Time
    //  4. TCP Handshake Time
    //  5. TLS resolution Time
    vector<vector<double>> separateMetricData(5, vector<double>());
    vector<long double> aggregates(5, 0.0);

    int validCount = 0;
    for (int i = 0; i < responseTimes.size(); i++) {
        if (responseTimes[i].total_time < 0)  // skip failed requests
            continue;

        validCount++;

        auto& [timeStamp, total_time, server_latency, dns_time, tcp_time,
               tls_time] = responseTimes[i];

        vector<double> temp = {total_time, server_latency, dns_time, tcp_time,
                               tls_time};

        for (int j = 0; j < 5; j++) {
            separateMetricData[j].push_back(temp[j]);
            aggregates[j] += temp[j];
        }
    }

    vector<double> averagaes(5, 0.0);
    for (int i = 0; i < 5; i++) {
        averagaes[i] = (aggregates[i] / validCount);
        sort(separateMetricData[i].begin(), separateMetricData[i].end());
    }

    int p95_index = (int)ceil(0.95 * (double)validCount) - 1;
    int p99_index = (int)ceil(0.99 * (double)validCount) - 1;

    vector<vector<double>> results(5, vector<double>(3, 0.0));

    for (int i = 0; i < 5; i++) {
        results[i][0] = averagaes[i];
        results[i][1] = separateMetricData[i][p95_index];
        results[i][2] = separateMetricData[i][p99_index];
    }

    return results;
}

// 1. The Mutex: Protects our shared data
std::mutex metrics_mutex;

// The Worker Function: What each thread will execute
void hammerWorker(const string& URL, int requests_per_thread,
                  vector<Metric>& global_results) {
    // Thread-local storage to prevent locking the mutex on every single request
    vector<Metric> local_results;
    local_results.reserve(requests_per_thread);

    // Perform the network requests completely independently
    for (int i = 0; i < requests_per_thread; i++) {
        // Assuming makeApiCall(URL) currently returns a vector<double>
        local_results.push_back(makeApiCall(URL));
    }

    // --- CRITICAL SECTION ---
    // Now that the thread is done, it locks the mutex to safely merge its
    // local data into the shared global vector.
    lock_guard<mutex> lock(metrics_mutex);
    global_results.insert(global_results.end(), local_results.begin(),
                          local_results.end());
}

// manager function to spin up worker threads and assign tasks to them
vector<Metric> hammerURL(const string& URL, int requests_per_thread,
                         int num_threads) {
    vector<Metric> global_results;
    global_results.reserve(requests_per_thread * num_threads);

    vector<thread> threads;

    // Spawn the OS threads
    for (int i = 0; i < num_threads; i++) {
        // in cpp, threads start running the moment they are created, so in the
        // line below, thread creation, starting thread execution, and storage
        // in threads vector all happens in one go.
        threads.emplace_back(hammerWorker, ref(URL), requests_per_thread,
                             ref(global_results));
    }

    // Block the main thread until all worker threads have finished their
    // execution
    // to "join" a thread means to wait until thread's work is done.
    // consequently, a "joinable" thread is simply checking if a thread is
    // actually running so we dont end up waiting for a thread that is already
    // joined or unable to join due to other reasons
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return global_results;
}

void displayResults(vector<vector<double>>& results) {
    vector<string> metricNames = {"Total Time", "Server Latency",
                                  "DNS Lookup Time", "TCP Handshake Time",
                                  "TLS resolution Time"};

    for (int i = 0; i < 5; i++) {
        cout << "\nMetric : " << metricNames[i] << " \n";
        cout << "Average : " << results[i][0] << " ms | P95 : " << results[i][1]
             << " ms | P99 : " << results[i][2] << " ms\n";
    }
}

void exportResultsToJson(const vector<Metric>& rawData,
                         const vector<vector<double>>& summaryResults) {
    // 1. Auto-generate the filename using your existing timestamp function
    long long timestamp = getTimeStamp();
    string filename = "bomber-run-" + to_string(timestamp) + ".json";

    // Initialize the main JSON object
    nlohmann::ordered_json outputData;

    // 2. Populate the Summary Statistics
    vector<string> metricNames = {"Total Time", "Server Latency",
                                  "DNS Lookup Time", "TCP Handshake Time",
                                  "TLS Resolution Time"};

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

Config loadConfig(const string& filename) {
    ifstream configFile(filename);

    // 1. Check if the file exists and can be opened
    if (!configFile.is_open()) {
        cerr << "\n[FATAL ERROR] Could not find '" << filename
             << "' in the current directory.\n";
        cerr << "Please create this file to configure your test.\n";
        exit(EXIT_FAILURE);  // Immediately terminates the program with error
                             // code 1
    }

    nlohmann::json j;
    try {
        // 2. Parse the file into the JSON object
        configFile >> j;

        // 3. Extract the variables and map them to our struct
        Config config;
        config.url = j.at("url").get<string>();
        config.concurrent_requests = j.at("concurrent_requests").get<int>();
        config.requests_per_thread = j.at("requests_per_thread").get<int>();

        cout << "[INFO] Configuration loaded successfully.\n";
        cout << " -> Target URL: " << config.url << "\n";
        cout << " -> Threads: " << config.concurrent_requests << "\n";
        cout << " -> Requests per thread: " << config.requests_per_thread
             << "\n\n";

        return config;

    } catch (const nlohmann::json::exception& e) {
        // 4. Catch JSON parsing errors (e.g., missing commas, missing keys)
        cerr << "\n[FATAL ERROR] Failed to parse '" << filename << "'.\n";
        cerr << "JSON Error: " << e.what() << "\n";
        exit(EXIT_FAILURE);
    }
}

int main() {
    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Config config = loadConfig("bomber-config.json");

    vector<Metric> responseTimes = hammerURL(
        config.url, config.requests_per_thread, config.concurrent_requests);

    vector<vector<double>> results = formulateResults(responseTimes);

    displayResults(results);

    exportResultsToJson(responseTimes, results);
    // Global cleanup
    curl_global_cleanup();

    return 0;
}