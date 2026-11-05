#include "Benchmark.h"
#include "model/Errors.h"
#include <QElapsedTimer>
#include <algorithm>
#include <random>
namespace clinic::algorithms {
std::vector<BenchmarkResult> benchmarkSorts(const PatientView &input, SortField field,
                                            int repeats) {
    if (input.empty() || input.size() > 10000 || repeats < 3 || repeats > 9 || repeats % 2 == 0)
        throw model::ValidationError("Use 1-10000 records and an odd repeat count from 3 to 9.");
    std::vector<BenchmarkResult> rows;
    auto sorted = input;
    sortPatients(sorted, field, SortAlgorithm::Merge);
    for (const auto &order : {QString("Sorted"), QString("Reversed"), QString("Shuffled")}) {
        auto base = sorted;
        if (order == "Reversed")
            std::reverse(base.begin(), base.end());
        if (order == "Shuffled") {
            std::mt19937 rng(2026);
            std::shuffle(base.begin(), base.end(), rng);
        }
        for (auto algorithm : {SortAlgorithm::Insertion, SortAlgorithm::Merge}) {
            // Warm up once; input preparation and copying are outside measured time.
            auto warmup = base;
            sortPatients(warmup, field, algorithm);
            std::vector<double> timings;
            Metrics metrics;
            for (int run = 0; run < repeats; ++run) {
                auto copy = base;
                Metrics current;
                QElapsedTimer timer;
                timer.start();
                sortPatients(copy, field, algorithm, &current);
                timings.push_back(timer.nsecsElapsed() / 1000000.0);
                metrics = current;
            }
            insertionSort(timings, [](double a, double b) { return a < b; });
            rows.push_back(
                {order, algorithm, static_cast<int>(base.size()), timings[repeats / 2], metrics});
        }
    }
    return rows;
}
} // namespace clinic::algorithms