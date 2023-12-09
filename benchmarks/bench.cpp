#include <benchmark/benchmark.h>

#include "../distance.hpp"

#include <random>
#include <array>
#include <stdexcept>
#include <cstring>

std::string random(size_t size, double q = .5) {
    std::mt19937 engine(1337);
    std::bernoulli_distribution bernoulli(q);
    std::string out; out.reserve(size);
    for (auto i = 0u; i != size; ++i) {
        out.push_back(bernoulli(engine) + '0');
    }

    return out;
}

template <typename Fn, typename Challenge>
static void BM_process(benchmark::State& state, Fn fn, Challenge challenge) {

    for (auto _ : state) {
        fn(challenge);
    }

    state.SetItemsProcessed(static_cast<int64_t>(challenge.size() * state.iterations()));
}

//static constexpr auto L1_CACHE_SIZE = 32 * 1024;
//static constexpr auto LOAD_TEST_SIZE = L1_CACHE_SIZE * 8;

std::vector<bool> wrapperBool(std::string const& v) {
    std::vector<bool> vec; vec.reserve(v.size());
    for (auto c : v) {
        vec.emplace_back(c - '0');
    }
    return vec;
}

BoolVector wrapperCustomBool(std::string const& v) {
    BoolVector vec(v.size());
    for (auto i = 0u; i != v.size(); ++i) {
        vec.set(i, v[i] - '0');
    }
    return vec;
}

std::vector<uint8_t> wrapperUint(std::string const& v) {
    std::vector<uint8_t> vec; vec.reserve(v.size());
    for (auto c : v) {
        vec.emplace_back(c - '0');
    }
    return vec;
}

static inline std::string MID_CHALLENGE = random(45);
static inline std::string MID_CHALLENGE_R = random(45, 0.2);
static inline std::string LONG1_CHALLENGE = random(8 * 16 * 80 + 5); // 1285 = 1 Kb
static inline std::string LONG1_CHALLENGE_R = random(8 * 16 * 80 + 5, 0.2); // 1285 = 1 Kb
static inline std::string LONG30_CHALLENGE = random(8 * 1024 * 30 + 11); // 30 Kb
static inline std::string LONG30_CHALLENGE_R = random(8 * 1024 * 30 + 11, 0.2); // 30 Kb
static inline std::string INF_CHALLENGE = random(8 * 1024 * 120); // 120 Kb
static inline std::string INF_CHALLENGE_R = random(8 * 1024 * 120, 0.2); // 120 Kb
static inline std::string INF_CHALLENGE_RR = random(8 * 1024 * 120, 0.05); // 120 Kb

#define DEF_BENCH(name, fn, wrapper) \
BENCHMARK_CAPTURE(BM_process, EQ_0_ ## name, fn, wrapper(MID_CHALLENGE)); \
BENCHMARK_CAPTURE(BM_process, R_0_ ## name, fn, wrapper(MID_CHALLENGE_R)); \
BENCHMARK_CAPTURE(BM_process, EQ_1_ ## name, fn, wrapper(LONG1_CHALLENGE)); \
BENCHMARK_CAPTURE(BM_process, R_1_ ## name, fn, wrapper(LONG1_CHALLENGE_R)); \
BENCHMARK_CAPTURE(BM_process, EQ_30_ ## name, fn, wrapper(LONG30_CHALLENGE)); \
BENCHMARK_CAPTURE(BM_process, R_30_ ## name, fn, wrapper(LONG30_CHALLENGE_R)); \
BENCHMARK_CAPTURE(BM_process, EQ_120_ ## name, fn, wrapper(INF_CHALLENGE)); \
BENCHMARK_CAPTURE(BM_process, R_120_ ## name, fn, wrapper(INF_CHALLENGE_R)); \
BENCHMARK_CAPTURE(BM_process, RR_120_ ## name, fn, wrapper(INF_CHALLENGE_RR));

DEF_BENCH(Slow, distanceSlow, wrapperBool);
DEF_BENCH(UintS, distanceUintSlow, wrapperUint);
DEF_BENCH(UintBranchLess, distanceUintSlowBranchLess, wrapperUint);
DEF_BENCH(MemoizedS, distanceMemoized, wrapperCustomBool);
DEF_BENCH(MemoizedSAligned, distanceMemoizedAligned, wrapperCustomBool);
DEF_BENCH(MemoizedBranchLess, distanceMemoizedBranchLess, wrapperCustomBool);


BENCHMARK_MAIN();