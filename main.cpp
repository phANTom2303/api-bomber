#include <curl/curl.h>

#include <algorithm>
#include <iostream>
#include <json.hpp>
#include <mutex>
#include <string>
#include <thread>

using namespace std;

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

vector<double> makeApiCall(const string& URL) {
    CURL* curl;
    CURLcode res;
    string readBuffer;  // This will hold the raw data

    // Initialize an easy session
    curl = curl_easy_init();
    vector<double> result;
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

            double dns_time_ms = dns_time_us / 1000.0;
            double tcp_time_ms = (tcp_time_us - dns_time_us) / 1000.0;
            double tls_time_ms = (tls_time_us - tcp_time_us) / 1000.0;
            double server_latency_ms = (ttfb_us - tls_time_us) / 1000.0;
            double total_time_ms = total_time_us / 1000.0;

            result = {total_time_ms, server_latency_ms, dns_time_ms,
                      tcp_time_ms, tls_time_ms};
            // cout << readBuffer << endl;
        }

        // Always cleanup when done
        curl_easy_cleanup(curl);
    }

    return result;
}

void formulateAndDisplayResults(vector<vector<double>>& responseTimes) {
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
        if (responseTimes[i][0] < 0)  // skip failed requests
            continue;

        validCount++;
        for (int j = 0; j < 5; j++) {
            separateMetricData[j].push_back(responseTimes[i][j]);
            aggregates[j] += responseTimes[i][j];
        }
    }

    vector<double> averagaes(5, 0.0);
    for (int i = 0; i < 5; i++) {
        averagaes[i] = (aggregates[i] / validCount);
        sort(separateMetricData[i].begin(), separateMetricData[i].end());
    }

    int p95_index = (int)ceil(0.95 * (double)validCount) - 1;
    int p99_index = (int)ceil(0.99 * (double)validCount) - 1;

    vector<string> metricNames = {"Total Time", "Server Latency",
                                  "DNS Lookup Time", "TCP Handshake Time",
                                  "TLS resolution Time"};

    for (int i = 0; i < 5; i++) {
        cout << "Metric : " << metricNames[i] << "\n";
        cout << "Average : " << averagaes[i] << " ms\n";
        cout << "95th Percentile : " << separateMetricData[i][p95_index]
             << " ms\n";
        cout << "99th Percentile : " << separateMetricData[i][p99_index]
             << " ms\n";
        cout << "\n";
    }
}

// 1. The Mutex: Protects our shared data
std::mutex metrics_mutex;

// The Worker Function: What each thread will execute
void hammerWorker(const string& URL, int requests_per_thread,
                  vector<vector<double>>& global_results) {
    // Thread-local storage to prevent locking the mutex on every single request
    vector<vector<double>> local_results;
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
vector<vector<double>> hammerURL(const string& URL, int total_requests,
                                 int num_threads) {
    vector<vector<double>> global_results;
    global_results.reserve(total_requests);

    vector<thread> threads;

    int requests_per_thread = total_requests / num_threads;

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

int main() {
    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    string URL = "https://jsonplaceholder.typicode.com/todos/1";

    vector<vector<double>> responseTimes =
        hammerURL(URL, 480, 12);  // 100 requests across 4 threads

    formulateAndDisplayResults(responseTimes);

    // Global cleanup
    curl_global_cleanup();

    return 0;
}