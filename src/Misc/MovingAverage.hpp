#include <array>
#include <numeric>

// ChatGPT wrote this on 12/13/2022
template <typename T, std::size_t N>
class MovingAverage {
public:
    MovingAverage() : sum_(0) {}

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

    [[nodiscard]] auto average() const -> T { return sum_ / count_; }

private:
    std::array<T, N> data_;
    T sum_;
    std::size_t index_ = 0;
    // Used for the startup case where there are not enough elements yet
    std::size_t count_ = 0;
};
