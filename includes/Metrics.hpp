#pragma once
#include <vector>
#include "BomberTypes.hpp"

long long getTimeStamp();
std::vector<std::vector<double>> formulateResults(std::vector<Metric>& responseTimes);
void displayResults(std::vector<std::vector<double>>& results);
void exportResultsToJson(const std::vector<Metric>& rawData, const std::vector<std::vector<double>>& summaryResults);