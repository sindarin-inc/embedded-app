#include <array>
#include <cmath>
#include <numeric>

// ChatGPT wrote this on 12/13/2022
template <typename T, std::size_t N>
class MovingAverage {
public:
    MovingAverage(T inertiaThreshold = 0)
        : sum_(0), inertiaThreshold_(inertiaThreshold), lastValidAverage_(0) {}

    void add(T value) {
        // Remove the oldest value from the sum
        sum_ -= data_[index_];
        // Add the new value to the sum
        sum_ += value;
        // Store the new value
        data_[index_] = value;
        // Increment the index for the next value
        index_ = (index_ + 1) % N;
        count_ = std::min(count_ + 1, N);
    }

    [[nodiscard]] auto average() -> T {
        T currentAverage = sum_ / count_;
        // Apply inertia: only update the last_valid_average if the difference exceeds the threshold
        if (std::abs(currentAverage - lastValidAverage_) >= inertiaThreshold_) {
            lastValidAverage_ = currentAverage;
        }
        return lastValidAverage_;
    }

private:
    std::array<T, N> data_;
    T sum_;
    std::size_t index_ = 0;
    // Used for the startup case where there are not enough elements yet
    std::size_t count_ = 0;
    T inertiaThreshold_;
    T lastValidAverage_;
};
