#include <curl/curl.h>

#include <algorithm>
#include <chrono>
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

void makeApiCall(string& URL) {
    CURL* curl;
    CURLcode res;
    string readBuffer;  // This will hold the raw data

    // Initialize an easy session
    curl = curl_easy_init();

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
        }

        // Always cleanup when done
        curl_easy_cleanup(curl);
    }
}

vector<double> hammerURL(string& URL) {
    vector<double> responseTimes;

    for (int i = 0; i < 10; i++) {
        auto start = chrono::high_resolution_clock::now();

        makeApiCall(URL);

        auto stop = chrono::high_resolution_clock::now();

        auto duration_obj =
            chrono::duration_cast<chrono::microseconds>(stop - start);

        auto actualDuration = duration_obj.count();

        double durationInMS = (actualDuration / 1000.0);

        responseTimes.push_back(durationInMS);
    }

    return responseTimes;
}
int main() {
    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    string URL = "https://jsonplaceholder.typicode.com/todos/1";

    vector<double> responseTimes = hammerURL(URL);
    for (double& durationInMS : responseTimes)
        cout << "Duration = " << durationInMS << " ms" << endl;

    // Global cleanup
    curl_global_cleanup();

    return 0;
}