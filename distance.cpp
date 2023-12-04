#include "distance.hpp"

#include "simd.hpp"

#include <cassert>
#include <array>
#include <iostream>


void distanceSlow(std::vector<bool>& input) {
    assert(!input.empty());

    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;

    size_t currentSeqSize = 0;
    for (auto i = 0u; i != input.size(); ++i) {
        auto t = input[i];
        if (t == 1) {
            if (longestSeqSize < currentSeqSize) {
                longestSeqSize = currentSeqSize;
                longestSeqPos = i - currentSeqSize;
            }
            currentSeqSize = 0;
        } else {
            ++currentSeqSize;
        }
    }

    if (longestSeqSize < currentSeqSize) {
        input.back() = true;
    } else if (longestSeqPos == 0) {
        input[longestSeqPos] = true;
    } else {
        input[longestSeqPos + longestSeqSize / 2] = true;
    }
}


void distanceUintSlow(std::vector<uint8_t>& input) {
    assert(!input.empty());

    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;

    size_t currentSeqSize = 0;
    for (auto i = 0u; i != input.size(); ++i) {
        auto t = input[i];
        if (t == 1) {
            if (longestSeqSize < currentSeqSize) {
                longestSeqSize = currentSeqSize;
                longestSeqPos = i - currentSeqSize;
            }
            currentSeqSize = 0;
        } else {
            ++currentSeqSize;
        }
    }

    if (longestSeqSize < currentSeqSize) {
        input.back() = true;
    } else if (longestSeqPos == 0) {
        input[longestSeqPos] = true;
    } else {
        input[longestSeqPos + longestSeqSize / 2] = true;
    }
}


constexpr std::tuple<uint8_t, uint8_t, uint8_t> analyzeBits(uint8_t value) {
    assert(value != 0);
    uint8_t firstOne = 0, longestZero = 0, lastOne = 0;
    uint8_t currentZero = 0;
    bool foundOne = false;

    for (uint8_t i = 0; i < 8; ++i) {
        if (value & (1 << i)) {
            if (!foundOne) {
                firstOne = i;
                foundOne = true;
            }
            longestZero = std::max(longestZero, currentZero);
            currentZero = 0;
            lastOne = i;
        } else {
            if (foundOne) {
                currentZero++;
            }
        }
    }

    return {firstOne, longestZero, 7 - lastOne};
}

static constexpr auto UINT8_SIZE = (1 << 8) - 1;

constexpr auto gen(){
    std::array<std::tuple<uint8_t, uint8_t, uint8_t>, UINT8_SIZE + 1> out{};
    out[0] = {8, 8, 8};
    for (uint8_t i = UINT8_SIZE; i != 0; --i) {
        out[i] = analyzeBits(i);
    }
    return out;
};

// return prefix seq, internal seq, suffix seq
std::tuple<uint8_t, uint8_t, uint8_t> process8(uint8_t n) {
    static constexpr auto cached = gen();
    return cached[n];
}


void findInChunk(BoolVector& input, size_t pos) {
    assert(pos % 8 == 0);
    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;
    size_t current = 0;
    for (auto i = pos; i != pos + 8; ++i) {
        auto t = input.get(i);
        if (t == 1) {
            if (longestSeqSize < current) {
                longestSeqSize = current;
                longestSeqPos = i - current;
            }
            current = 0;
        } else {
            ++current;
        }
    }
//    std::cout << "chunk:" << longestSeqSize << std::endl;
    assert(longestSeqPos + longestSeqSize / 2);
    input.set(longestSeqPos + longestSeqSize / 2, true);
}

void distanceMemoized(BoolVector& input) {
    auto const size = input.size();
    auto const chunks = input.fullChunks();

    size_t current = 0;
    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;
    bool inChunk = false;

    for (auto i = 0u; i != chunks; ++i) {
        auto [np, longest, ns] = process8(input.rawData()[i]);
        if (np == 8) {
            current += 8;
        } else {
            auto lp = np + current;
            if (lp > longestSeqSize) [[unlikely]] {
                longestSeqSize = lp;
                assert(i * 8 + np >= longestSeqSize);
                longestSeqPos = i * 8 + np - longestSeqSize;
                inChunk = false;
            }
            if (longest > longestSeqSize) [[unlikely]] {
                inChunk = true;
                longestSeqSize = longest;
                longestSeqPos = i * 8;
            }
            current = ns;
        }
    }

    if (size % 8 != 0) {
        for (auto j = chunks * 8; j != size; ++j) {
            auto t = input.get(j);
            if (t == 1) {
                if (longestSeqSize < current) {
                    longestSeqSize = current;
                    assert(j >= current);
                    longestSeqPos = j - current;
                    inChunk = false;
                }
                current = 0;
            } else {
                ++current;
            }
        }
    }

    if (longestSeqSize < current) {
        assert(input.get(size - 1) == false);
        input.set(size - 1, true);
//        std::cout << "end:" << longestSeqSize << std::endl;
    } else if (longestSeqPos == 0) {
        if (!inChunk) {
            assert(input.get(0) == false || longestSeqSize == 0);
            input.set(0, true);
//            std::cout << "0:" << longestSeqSize << std::endl;
        } else {
            findInChunk(input, longestSeqPos);
        }
    } else {
        if (!inChunk) {
            assert(input.get(longestSeqPos + longestSeqSize / 2) == false);
            input.set(longestSeqPos + longestSeqSize / 2, true);
            //std::cout << "mid:" << longestSeqSize << std::endl;
        } else {
            findInChunk(input, longestSeqPos);
        }
    }
}

void distanceMemoizedBranchLess(BoolVector& input) {
    auto const size = input.size();
    auto const chunks = input.fullChunks();

    uint32_t current = 0;
    uint32_t longestSeqSize = 0;
    uint32_t longestSeqPos = 0;

    static constexpr uint32_t IN_CHUNK_BIT = (1ull << 31);
    for (auto i = 0u; i != chunks; ++i) {
        auto [np, longest, ns] = process8(input.rawData()[i]);

        auto leftValue = np + current;

        using StateT = std::tuple<uint32_t, uint32_t, uint32_t>;
        StateT all{current + 8, longestSeqSize, longestSeqPos};
        StateT noChanges{ns, longestSeqSize, longestSeqPos};
        StateT leftUpdate{ns, leftValue, i * 8 - current};
        StateT insideUpdate{ns, longest, i * 8 | IN_CHUNK_BIT};

        std::array<StateT, 5> results{all, noChanges, leftUpdate, insideUpdate, insideUpdate};

        auto hasOnes = np != 8;
        auto leftTrigger = np + current > longestSeqSize;
        auto triggerInside = longest > longestSeqSize && longest > np + current;
        std::tie(current, longestSeqSize, longestSeqPos)
            = results[hasOnes * (1 + leftTrigger + triggerInside * 2)];
    }

    if (size % 8 != 0) {
        for (auto j = chunks * 8; j != size; ++j) {
            auto t = input.get(j);
            if (t == 1) {
                if (longestSeqSize < current) {
                    longestSeqSize = current;
                    assert(j >= current);
                    longestSeqPos = j - current;
                }
                current = 0;
            } else {
                ++current;
            }
        }
    }

    if (longestSeqSize < current) {
//        std::cout << "end:" << longestSeqSize << std::endl;
        assert(input.get(size - 1) == false);
        input.set(size - 1, true);
    } else if (longestSeqPos == 0) {
//        std::cout << "start:" << longestSeqSize << std::endl;
        assert(input.get(0) == false || longestSeqSize == 0);
        input.set(0, true);
    } else if (longestSeqPos & IN_CHUNK_BIT) {
        findInChunk(input, longestSeqPos & ~IN_CHUNK_BIT);
    } else {
//        std::cout << "mid:" << longestSeqPos + longestSeqSize / 2 << std::endl;
        assert(input.get(longestSeqPos + longestSeqSize / 2) == false);
        input.set(longestSeqPos + longestSeqSize / 2, true);
    }
}


template <size_t Ind> requires(Ind < 3)
constexpr auto genSingle(){
    std::array<uint8_t, UINT8_SIZE + 1> out{};
    auto sourceTable = gen();
    for (uint16_t i = 0; i != UINT8_SIZE + 1; ++i) {
        out[i] = std::get<Ind>(sourceTable[i]);
    }
    return out;
};


#ifdef __AVX512F__

void distanceMemoizedAVX(BoolVector& input) {
    auto const size = input.size();
    auto const chunks = input.fullChunks();

    size_t current = 0;
    size_t longestSeqSize = 0;
    size_t longestSeqPos = 0;
    bool inChunk = false;

    auto i = 0u;
    if (chunks > 64) {
        auto const fixedChunks = chunks - (chunks % 64);
        for (; i != fixedChunks; i += 64) {
//            auto dataReg = _mm512_load_epi64(input.rawData() + i);
//            auto lp = shuffle(dataReg, genSingle<0>());
//            auto hp = shuffle(dataReg, genSingle<2>());

        }
    }

    for (; i + 1 != chunks; ++i) {
        auto [np, longest, ns] = process8(input.rawData()[i]);
        if (np == 8) {
            current += 8;
        } else {
            auto lp = np + current;
            if (lp > longestSeqSize) [[unlikely]] {
                longestSeqSize = lp;
                assert(i * 8 + np >= longestSeqSize);
                longestSeqPos = i * 8 + np - longestSeqSize;
                inChunk = false;
            }
            if (longest > longestSeqSize) [[unlikely]] {
                inChunk = true;
                longestSeqSize = longest;
                longestSeqPos = i * 8;
            }
            current = ns;
        }
    }

    for (auto j = (chunks - 1) * 8; j != size; ++j) {
        auto t = input.get(j);
        if (t == 1) {
            if (longestSeqSize < current) {
                longestSeqSize = current;
                assert(j >= current);
                longestSeqPos = j - current;
                inChunk = false;
            }
            current = 0;
        } else {
            ++current;
        }
    }

    if (longestSeqSize < current) {
        assert(input.get(size - 1) == false);
        input.set(size - 1, true);
        //std::cout << "end:" << longestSeqSize << std::endl;
    } else if (longestSeqPos == 0) {
        if (!inChunk) {
            assert(input.get(0) != false && longestSeqSize != 0);
            input.set(0, true);
            //std::cout << "0:" << longestSeqSize << std::endl;
        } else {
            findInChunk(input, longestSeqPos);
        }
    } else {
        if (!inChunk) {
            assert(input.get(longestSeqPos + longestSeqSize / 2) == false);
            input.set(longestSeqPos + longestSeqSize / 2, true);
            //std::cout << "mid:" << longestSeqSize << std::endl;
        } else {
            findInChunk(input, longestSeqPos);
        }
    }
}

#endif