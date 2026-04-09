#pragma once
#include <string>
#include <vector>
#include "BomberTypes.hpp"

std::vector<Metric> hammerURL(const std::string& URL, int requests_per_thread, int num_threads);