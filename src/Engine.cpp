#include "Engine.hpp"
#include "Metrics.hpp" 
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <iostream>

using namespace std;

mutex metrics_mutex;

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
