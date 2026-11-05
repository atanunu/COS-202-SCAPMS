#pragma once
#include "Algorithms.h"
#include <QString>
namespace clinic::algorithms {
struct BenchmarkResult {
    QString order;
    SortAlgorithm algorithm;
    int records = 0;
    double medianMs = 0;
    Metrics metrics;
};
std::vector<BenchmarkResult> benchmarkSorts(const PatientView &, SortField, int repeats = 5);
} // namespace clinic::algorithms