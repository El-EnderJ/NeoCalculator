/**
 * StatsEngine.cpp — Statistical computation engine for NumOS.
 */

#include "StatsEngine.h"

void StatsEngine::clear() {
    _data.clear();
}

void StatsEngine::setData(const std::vector<DataPoint>& data) {
    _data = data;
}

size_t StatsEngine::count() const {
    double n = 0;
    for (auto& dp : _data) n += dp.frequency;
    return static_cast<size_t>(n);
}

double StatsEngine::getSum() const {
    double s = 0;
    for (auto& dp : _data) s += dp.value * dp.frequency;
    return s;
}

double StatsEngine::getMean() const {
    size_t n = count();
    if (n == 0) return 0.0;
    return getSum() / static_cast<double>(n);
}

double StatsEngine::getMedian() const {
    size_t n = count();
    if (n == 0) return 0.0;

    // Expand into sorted list of values (by frequency)
    std::vector<double> expanded;
    expanded.reserve(n);
    for (auto& dp : _data) {
        int f = static_cast<int>(dp.frequency);
        for (int i = 0; i < f; ++i) expanded.push_back(dp.value);
    }
    std::sort(expanded.begin(), expanded.end());

    size_t sz = expanded.size();
    if (sz % 2 == 1) {
        return expanded[sz / 2];
    } else {
        return (expanded[sz / 2 - 1] + expanded[sz / 2]) / 2.0;
    }
}

double StatsEngine::getStandardDeviation() const {
    size_t n = count();
    if (n <= 1) return 0.0;

    double mean = getMean();
    double sumSqDiff = 0;
    for (auto& dp : _data) {
        double diff = dp.value - mean;
        sumSqDiff += diff * diff * dp.frequency;
    }
    // Population standard deviation (σ)
    return std::sqrt(sumSqDiff / static_cast<double>(n));
}

double StatsEngine::getMin() const {
    if (_data.empty()) return 0.0;
    double m = _data[0].value;
    for (size_t i = 1; i < _data.size(); ++i) {
        if (_data[i].value < m) m = _data[i].value;
    }
    return m;
}

double StatsEngine::getMax() const {
    if (_data.empty()) return 0.0;
    double m = _data[0].value;
    for (size_t i = 1; i < _data.size(); ++i) {
        if (_data[i].value > m) m = _data[i].value;
    }
    return m;
}
