#include <gtest/gtest.h>

#include <source_location>
#include <random>

#include "../distance.hpp"

using sv = std::string_view;

template <void(*fn)(std::vector<bool>&)>
struct WrapperVectorBool {
    auto operator()(std::string& v) const {
        std::vector<bool> vec; vec.reserve(v.size());
        for (auto c : v) {
            vec.emplace_back(c - '0');
        }
        fn(vec);
        assert(vec.size() == v.size());
        for (auto i = 0u; i != vec.size(); ++i) {
            v[i] = static_cast<char>(vec[i]) + '0';
        }
    }
};

template <void(*fn)(std::vector<uint8_t>&)>
struct WrapperVector {
    auto operator()(std::string& v) const {
        std::vector<uint8_t> vec; vec.reserve(v.size());
        for (auto c : v) {
            vec.emplace_back(c - '0');
        }
        fn(vec);
        assert(vec.size() == v.size());
        for (auto i = 0u; i != vec.size(); ++i) {
            v[i] = static_cast<char>(vec[i]) + '0';
        }
    }
};

template <void(*fn)(BoolVector&)>
struct WrapperCustomBool {
    auto operator()(std::string& v) const {
        BoolVector vec(v.size());
        for (auto i = 0u; i != v.size(); ++i) {
            vec.set(i, v[i] - '0');
        }
        fn(vec);
        assert(vec.size() == v.size());
        for (auto i = 0u; i != vec.size(); ++i) {
            v[i] = static_cast<char>(vec.get(i)) + '0';
        }
    }
};

using SlowT = WrapperVectorBool<distanceSlow>;
using SlowUintT = WrapperVector<distanceUintSlow>;
using SlowUintBranchLessT = WrapperVector<distanceUintSlowBranchLess>;
using MemoizedT = WrapperCustomBool<distanceMemoized>;
using MemoizedAlignedT = WrapperCustomBool<distanceMemoizedAligned>;
using MemoizedBranchLessT = WrapperCustomBool<distanceMemoizedBranchLess>;

template <typename Fn>
class DistanceTest : public ::testing::Test {
public:
    auto test(std::string str, std::string ans = "", std::source_location current = std::source_location::current()) {
        auto backupStr = str;
        if (ans.empty()) {
            ans = str;
            SlowT{}(ans);
        }
        auto onesInSrc = onesInStr(str);
        m_fn(str);
        auto onesInAnswer = onesInStr(str);
        EXPECT_EQ(onesInAnswer, (onesInSrc == str.size() ? onesInSrc : onesInSrc + 1)) << location(current) << " str.size(): " << str.size();
        EXPECT_EQ(str, ans) << location(current) << " [" << backupStr << "]";
    }
private:
    static unsigned onesInStr(std::string_view s) {
        return std::count(s.begin(), s.end(), '1');
    }
    std::string makeStr(std::string const& v, size_t l, size_t r) {
        auto prefix = std::string("1", l);
        auto suffix = std::string("1", l);
        return prefix + v + suffix;
    }

    static std::string location(std::source_location current) noexcept {
        return std::string(current.file_name()) + ":" + std::to_string(current.line());
    }

    Fn m_fn{};
};

TYPED_TEST_SUITE_P(DistanceTest);

TYPED_TEST_P(DistanceTest, Tests) {
    this->test("100001", "100101");
    this->test("10001", "10101");

    this->test("1000", "1001");
    this->test("0001", "1001");
    this->test("00001", "10001");
    this->test("10000", "10001");
    this->test("00000", "00001");

    this->test("10101", "11101");
    this->test("10010001", "10010101");
    this->test("10010001001", "10010101001");

    this->test("0000010000", "1000010000");
    this->test("00000100000", "10000100000");
    this->test("000001000001", "100001000001");

    this->test("1", "1");
    this->test("0", "1");
    this->test("00", "01");
    this->test("10", "11");
    this->test("01", "11");
    this->test("101", "111");
    this->test("111", "111");
    this->test("000", "001");
    this->test("010", "110");
}

std::string random(size_t minSize, size_t maxSize, double q = .5) {
    std::random_device rd;
    std::mt19937 engine(rd());
    std::bernoulli_distribution bernoulli(q);
    std::uniform_int_distribution uniform(minSize, maxSize);
    auto strSize = uniform(engine);
    std::string out; out.reserve(strSize);
    for (auto i = 0u; i != strSize; ++i) {
        out.push_back(bernoulli(engine) + '0');
    }

    return out;
}

TYPED_TEST_P(DistanceTest, Big) {
    this->test("0110000101110010000000000100110000000011000000100000100000000001000001000110010010");
    this->test("00101001000010000011110000010000000100000000001100100100000000100000110010100001000010000001000000001100100000000000010010011000001100000000000111001000100100000100000001000000001001000001101010000100000000001001000000000010000000000100000000100011001000010001100000000000001010000011000000001000000000000000100000000000000101000000010100000011001001100101010101001000010000000000010000101011000000000000100000011000000000101010000000000010000000000000101000000100001001010000000000000000000100010000010000000000000100000000000000100001001010100000010010000000000000110100000000000000010000100010010000000010010110010100101001000001000000001110001100000000010000000000000000001000000001010011000000001000110000010101000100000000001000001010100000001010100000010000000100000000000000000000001010000001100010000010011");
}

TYPED_TEST_P(DistanceTest, Random) {
    static constexpr size_t TEST_CASES = 10'000;
    for (auto i = 0; i != TEST_CASES; ++i) {
        this->test(random(1, 1000, 0.2));
    }
    for (auto i = 0; i != TEST_CASES; ++i) {
        this->test(random(1, 1000, 0.5));
    }
}

REGISTER_TYPED_TEST_SUITE_P(DistanceTest, Tests, Big, Random);

INSTANTIATE_TYPED_TEST_SUITE_P(Slow, DistanceTest, SlowT);
INSTANTIATE_TYPED_TEST_SUITE_P(SlowUint, DistanceTest, SlowUintT);
INSTANTIATE_TYPED_TEST_SUITE_P(SlowUintBranchLess, DistanceTest, SlowUintBranchLessT);
INSTANTIATE_TYPED_TEST_SUITE_P(Memoized, DistanceTest, MemoizedT);
INSTANTIATE_TYPED_TEST_SUITE_P(MemoizedAlign, DistanceTest, MemoizedAlignedT);
INSTANTIATE_TYPED_TEST_SUITE_P(MemoizedBranchLess, DistanceTest, MemoizedBranchLessT);


