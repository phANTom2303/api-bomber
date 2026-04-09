#pragma once
#include <vector>
#include "BomberTypes.hpp"

long long getTimeStamp();
Result formulateResults(std::vector<Metric>& responseTimes);
void displayResults(Result);
void exportResultsToJson(const std::vector<Metric>& rawData, Result);