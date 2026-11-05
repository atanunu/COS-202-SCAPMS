#include "Algorithms.h"

namespace clinic::algorithms {
const model::PatientRecord *
linearSearchById(const std::vector<std::unique_ptr<model::PatientRecord>> &data, const QString &id,
                 Metrics *metrics) {
    for (const auto &patient : data) {
        if (metrics)
            ++metrics->comparisons;
        if (patient->id() == id)
            return patient.get();
    }
    return nullptr;
}
const model::PatientRecord *binarySearchById(const PatientView &sorted, const QString &id,
                                             Metrics *metrics) {
    // Precondition: ascending, case-sensitive ID order; GUI creates this view explicitly.
    size_t low = 0, high = sorted.size();
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (metrics)
            ++metrics->comparisons;
        const int comparison = QString::compare(sorted[middle]->id(), id, Qt::CaseSensitive);
        if (comparison == 0)
            return sorted[middle];
        if (comparison < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return nullptr;
}
PatientView filterPatients(const PatientView &data, const QString &text, bool byCategory) {
    PatientView matches;
    for (const auto *patient : data) {
        const auto field =
            byCategory ? model::categoryName(patient->category()) : patient->details().name;
        if (field.contains(text, Qt::CaseInsensitive))
            matches.push_back(patient);
    }
    return matches;
}
PatientView viewOf(const std::vector<std::unique_ptr<model::PatientRecord>> &data) {
    PatientView result;
    for (const auto &patient : data)
        result.push_back(patient.get());
    return result;
}
void sortPatients(PatientView &data, SortField field, SortAlgorithm algorithm, Metrics *metrics) {
    const auto less = [field](const model::PatientRecord *a, const model::PatientRecord *b) {
        switch (field) {
        case SortField::Name:
            return QString::compare(a->details().name, b->details().name, Qt::CaseInsensitive) < 0;
        case SortField::Age:
            return a->details().age < b->details().age;
        case SortField::Id:
            return QString::compare(a->id(), b->id(), Qt::CaseSensitive) < 0;
        }
        return false;
    };
    if (algorithm == SortAlgorithm::Insertion)
        insertionSort(data, less, metrics);
    else
        mergeSort(data, less, metrics);
}
} // namespace clinic::algorithms
