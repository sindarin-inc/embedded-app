#include <algorithm>

// ChatGPT wrote this 12/19/2022 (including the comment)
/**
 * Maps an input value from one range to another using linear interpolation.
 *
 * @param x The input value to be mapped.
 * @param inMin The minimum value of the input range.
 * @param inMax The maximum value of the input range.
 * @param outMin The minimum value of the output range.
 * @param outMax The maximum value of the output range.
 * @return The interpolated value in the output range.
 */
template <typename T>
auto MapRange(T x, T inMin, T inMax, T outMin, T outMax) -> T {
    T range = inMax - inMin;
    T t = (x - inMin) * (outMax - outMin) / range;
    if (outMin < outMax) {
        return std::clamp(outMin + t, outMin, outMax);
    } else {
        return std::clamp(outMin + t, outMax, outMin);
    }
}
