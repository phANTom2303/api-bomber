#include "ConfigLoader.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <json.hpp>

using namespace std;

Config loadConfig(const string& filename) {
    ifstream configFile(filename);
    if (!configFile.is_open()) {
        cerr << "\n[FATAL ERROR] Could not find '" << filename << "'.\n";
        exit(EXIT_FAILURE);
    }

    nlohmann::json j;
    try {
        configFile >> j;
        Config config;
        config.url = j.at("url").get<string>();
        config.concurrent_requests = j.at("concurrent_requests").get<int>();
        config.requests_per_thread = j.at("requests_per_thread").get<int>();

        cout << "[INFO] Configuration loaded successfully.\n";
        return config;
    } catch (const nlohmann::json::exception& e) {
        cerr << "\n[FATAL ERROR] Failed to parse '" << filename << "'.\n" << e.what() << "\n";
        exit(EXIT_FAILURE);
    }
}