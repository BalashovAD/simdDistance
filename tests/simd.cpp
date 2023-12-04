#include <gtest/gtest.h>

#include "../simd.hpp"

constexpr std::array<uint8_t, 256> createArray() {
    std::array<uint8_t, 256> arr{};
    for (size_t i = 0; i < arr.size(); ++i) {
        arr[i] = static_cast<uint8_t>(255 - i);
    }
    return arr;
}

constexpr std::array<uint8_t, 256> lookupTable = createArray();

#ifdef __AVX512F__

TEST(SIMD_lookup, Base) {

    std::array<uint8_t, 64> arr{};
    for (size_t i = 0; i < arr.size(); ++i) {
        arr[i] = i;
    }
    auto result = shuffle(_mm512_load_epi64(arr.data()), lookupTable);
    _mm512_store_epi64(arr.data(), result);

    for (auto i = 0u; i != arr.size(); ++i) {
        EXPECT_EQ(arr[i], 64 - i);
    }
}

#endif