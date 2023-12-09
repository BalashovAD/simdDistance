#pragma once

#include <immintrin.h>
#include <array>
#include <cstdint>

#ifdef AVX512F

static constexpr auto TABLE_SIZE = 256;

namespace {
[[gnu::always_inline]] inline __m512i load_value(uint8_t fill) {
    return _mm512_set1_epi8(fill);
}

inline __m512i blend(__m512i a, __m512i b, __mmask64 mask) {
    return _mm512_mask_blend_epi8(mask, a, b);
}

}

[[gnu::always_inline]] inline __m512i shuffle(__m512i src, std::array<uint8_t, TABLE_SIZE> lookupTable) {
    static constexpr auto REG_SIZE = 64;
    auto l1 = _mm512_load_epi64(lookupTable.data());
    auto l2 = _mm512_load_epi64(lookupTable.data() + REG_SIZE);
    auto grt2 = load_value(REG_SIZE);
    auto l3 = _mm512_load_epi64(lookupTable.data() + REG_SIZE * 2);
    auto grt3 = load_value(REG_SIZE * 2);
    auto l4 = _mm512_load_epi64(lookupTable.data() + REG_SIZE * 3);
    auto grt4 = load_value(REG_SIZE * 2);

    auto result1 = _mm512_shuffle_epi8(l1, src);
    auto result2 = _mm512_shuffle_epi8(l2, src);
    auto result3 = _mm512_shuffle_epi8(l3, src);
    auto result4 = _mm512_shuffle_epi8(l4, src);

    auto select2 = _mm512_cmpge_epi8_mask(src, grt2);
    auto select3 = _mm512_cmpge_epi8_mask(src, grt3);
    auto select4 = _mm512_cmpge_epi8_mask(src, grt4);

    auto fixedResult1 = blend(result1, result2, select2);
    auto fixedResult2 = blend(fixedResult1, result3, select3);
    auto fixedResult3 = blend(fixedResult2, result4, select4);

    return fixedResult3;
}


namespace {

inline __m256i blend(__m256i a, __m256i b, __mmask32 mask) {
    return _mm256_mask_blend_epi8(mask, a, b);
}

}

template <size_t TABLE_SIZE>
[[gnu::always_inline]] inline __m256i shuffle(__m256i src, std::array<uint8_t, TABLE_SIZE> lookupTable) {
    static constexpr auto REG_SIZE = 32;
    auto resultReg = _mm256_set1_epi8(0);

    for (auto i = 0u; i != TABLE_SIZE / REG_SIZE; ++i) {
        auto tableReg = _mm256_load_epi64(lookupTable.data() + i * REG_SIZE);
        auto resultTmpReg = _mm256_shuffle_epi8(tableReg, src);
        auto borderReg = _mm256_set1_epi8(i * REG_SIZE);
        auto selectReg = _mm256_cmpge_epi8_mask(src, borderReg);
        resultReg = blend(resultReg, resultTmpReg, selectReg);
    }

    return resultReg;
}

#endif
