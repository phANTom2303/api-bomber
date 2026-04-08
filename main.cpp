#include <curl/curl.h>

#include <algorithm>
#include <iostream>
#include <json.hpp>
#include <string>

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

vector<double> makeApiCall(string& URL) {
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
            cout << readBuffer << endl;
        }

        // Always cleanup when done
        curl_easy_cleanup(curl);
    }

    return result;
}

vector<vector<double>> hammerURL(string& URL) {
    vector<vector<double>> responseTimes;

    for (int i = 0; i < 10; i++) {
        responseTimes.push_back(makeApiCall(URL));
    }

    return responseTimes;
}
int main() {
    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    string URL = "https://jsonplaceholder.typicode.com/todos/1";

    vector<vector<double>> responseTimes = hammerURL(URL);
    for (vector<double>& data : responseTimes) {
        if (data[0] < 0)
            cout << "Failed" << endl;
        else {
            for (auto& d : data) cout << d << " ";
            cout << endl;
        }
    }

    // Global cleanup
    curl_global_cleanup();

    return 0;
}