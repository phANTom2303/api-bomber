#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <json.hpp>
#include <curl/curl.h>

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

void makeApiCall(string & URL){
    CURL* curl;
    CURLcode res;
    string readBuffer; // This will hold the raw data

    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize an easy session
    curl = curl_easy_init();
    
    if(curl) {
        // 1. Set the URL you want to GET
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

        // 2. Pass our custom callback function to handle incoming data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

        // 3. Pass the string variable to the callback function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // 4. Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if(res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        } else {
            // Print the raw data received
            cout << "--- Raw Data Received ---\n";
            cout << readBuffer << endl;
            cout << "-------------------------\n";
        }

        // Always cleanup when done
        curl_easy_cleanup(curl);
    }

    // Global cleanup
    curl_global_cleanup();
}
int main() {
    string URL = "https://jsonplaceholder.typicode.com/todos/";
   

    auto start = chrono::high_resolution_clock::now();

    makeApiCall(URL);

    auto stop = chrono::high_resolution_clock::now();

    auto duration_obj = chrono::duration_cast<chrono::microseconds>(stop - start);

    auto actualDuration = duration_obj.count();
    
    cout << "Duration = " << actualDuration << " microseconds" << endl;

    return 0;
}