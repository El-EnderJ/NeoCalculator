/**
 * StatsEngine.h — Statistical computation engine for NumOS.
 *
 * Processes paired data (values + frequencies) and computes:
 *   - Mean, Median, Standard Deviation, Min, Max, Sum, Count
 *
 * Pure C++ — no LVGL or Arduino dependencies.
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>

class StatsEngine {
public:
    struct DataPoint {
        double value;
        double frequency;   // ≥ 1
    };

    void clear();
    void setData(const std::vector<DataPoint>& data);
    size_t count() const;          ///< Total frequency (Σ freq)

    double getMean() const;
    double getMedian() const;
    double getStandardDeviation() const;
    double getMin() const;
    double getMax() const;
    double getSum() const;

    const std::vector<DataPoint>& data() const { return _data; }

private:
    std::vector<DataPoint> _data;
};
