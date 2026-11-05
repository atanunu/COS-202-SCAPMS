#pragma once
#include "model/PatientRecord.h"
#include <functional>
#include <vector>

namespace clinic::algorithms {
using PatientView = std::vector<const model::PatientRecord *>;
enum class SortField { Name, Id, Age };
enum class SortAlgorithm { Insertion, Merge };
struct Metrics {
    size_t comparisons = 0;
    size_t moves = 0;
    size_t maxRecursionDepth = 0;
};
const model::PatientRecord *
linearSearchById(const std::vector<std::unique_ptr<model::PatientRecord>> &, const QString &,
                 Metrics * = nullptr);
const model::PatientRecord *binarySearchById(const PatientView &, const QString &,
                                             Metrics * = nullptr);
PatientView filterPatients(const PatientView &, const QString &text, bool byCategory);
PatientView viewOf(const std::vector<std::unique_ptr<model::PatientRecord>> &);
void sortPatients(PatientView &, SortField, SortAlgorithm, Metrics * = nullptr);
// Explicit implementations, also reusable by attendance reports.
template <class T, class Less>
void insertionSort(std::vector<T> &data, Less less, Metrics *metrics = nullptr) {
    for (size_t i = 1; i < data.size(); ++i) {
        T value = std::move(data[i]);
        size_t j = i;
        while (j > 0) {
            if (metrics)
                ++metrics->comparisons;
            if (!less(value, data[j - 1]))
                break;
            data[j] = std::move(data[j - 1]);
            --j;
            if (metrics)
                ++metrics->moves;
        }
        data[j] = std::move(value);
    }
}
template <class T, class Less>
void mergeRange(std::vector<T> &data, std::vector<T> &buffer, size_t begin, size_t end, Less less,
                Metrics *metrics, size_t depth) {
    if (metrics && depth > metrics->maxRecursionDepth)
        metrics->maxRecursionDepth = depth;
    if (end - begin < 2)
        return; // Recursive base case: zero or one item.
    const size_t middle = begin + (end - begin) / 2;
    mergeRange(data, buffer, begin, middle, less, metrics, depth + 1);
    mergeRange(data, buffer, middle, end, less, metrics, depth + 1);
    size_t left = begin, right = middle, out = begin;
    while (left < middle && right < end) {
        if (metrics)
            ++metrics->comparisons;
        // Left wins ties, preserving stability.
        buffer[out++] = less(data[right], data[left]) ? data[right++] : data[left++];
    }
    while (left < middle)
        buffer[out++] = data[left++];
    while (right < end)
        buffer[out++] = data[right++];
    for (size_t i = begin; i < end; ++i) {
        data[i] = buffer[i];
        if (metrics)
            ++metrics->moves;
    }
}
template <class T, class Less>
void mergeSort(std::vector<T> &data, Less less, Metrics *metrics = nullptr) {
    if (data.empty())
        return;
    auto buffer = data;
    mergeRange(data, buffer, 0, data.size(), less, metrics, 1);
}
} // namespace clinic::algorithms
