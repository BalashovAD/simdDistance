#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <stdexcept>


void distanceSlow(std::vector<bool>& input);

void distanceUintSlow(std::vector<uint8_t>& input);

void distanceUintSlowBranchLess(std::vector<uint8_t>& input);

class BoolVector {
public:
    explicit BoolVector(size_t size)
        : m_size(size)
        , m_data((size + 7) / 8, 0) {}

    bool get(size_t index) const {
        checkRange(index);
        size_t byteIndex = index / 8;
        size_t bitIndex = index % 8;
        return (m_data[byteIndex] & (1 << bitIndex)) != 0;
    }

    void set(size_t index, bool value) {
        checkRange(index);
        size_t byteIndex = index / 8;
        size_t bitIndex = index % 8;
        if (value) {
            m_data[byteIndex] |= (1 << bitIndex);
        } else {
            m_data[byteIndex] &= ~(1 << bitIndex);
        }
    }

    const uint8_t* rawData() const {
        return m_data.data();
    }

    size_t size() const {
        return m_size;
    }

    size_t chunks() const {
        return m_data.size();
    }

    size_t fullChunks() const {
        return m_data.size() - (size() % 8 != 0);
    }

private:
    size_t m_size;
    std::vector<uint8_t> m_data;

    void checkRange(size_t index) const noexcept {
        if (index >= size()) {
//            throw std::out_of_range("Index out of range");
        }
    }
};


void distanceMemoized(BoolVector& input);
void distanceMemoizedAligned(BoolVector& input);

void distanceMemoizedBranchLess(BoolVector& input);

void distanceMemoizedAVX(BoolVector& input);