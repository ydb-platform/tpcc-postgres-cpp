#pragma once

#include "constants.h"

#include <cstdint>
#include <random>
#include <string>
#include <stop_token>
#include <atomic>

namespace NTPCC {

//-----------------------------------------------------------------------------

namespace NDetail {

// xorshift64* - tiny, fast, non-cryptographic PRNG. 64-bit state, 64-bit output.
// Period 2^64 - 1; ample for TPC-C data generation.
class TFastRng {
public:
    TFastRng() {
        std::random_device rd;
        uint64_t s = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
        State_ = s ? s : 0x9E3779B97F4A7C15ULL;
    }

    uint64_t Next() {
        uint64_t x = State_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        State_ = x;
        return x * 0x2545F4914F6CDD1DULL;
    }

private:
    uint64_t State_;
};

inline TFastRng& ThreadLocalFastRng() {
    thread_local TFastRng rng;
    return rng;
}

// Lemire's nearly-divisionless unbiased bounded random in [0, range).
// https://arxiv.org/abs/1805.10941
inline uint64_t BoundedRandom(uint64_t range) {
    if (range == 0) {
        return 0;
    }
    TFastRng& rng = ThreadLocalFastRng();
    uint64_t x = rng.Next();
    __uint128_t m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
    uint64_t l = static_cast<uint64_t>(m);
    if (l < range) {
        uint64_t t = (0ULL - range) % range;
        while (l < t) {
            x = rng.Next();
            m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
            l = static_cast<uint64_t>(m);
        }
    }
    return static_cast<uint64_t>(m >> 64);
}

} // namespace NDetail

// [from; to] inclusive range
inline size_t RandomNumber(size_t from, size_t to) {
    return from + static_cast<size_t>(NDetail::BoundedRandom(to - from + 1));
}

// Non-uniform random number generation as per TPC-C spec
inline int NonUniformRandom(int A, int C, int min, int max) {
    int randomNum = RandomNumber(0, A);
    int randomNum2 = RandomNumber(min, max);
    return (((randomNum | randomNum2) + C) % (max - min + 1)) + min;
}

inline int GetRandomCustomerID() {
    return NonUniformRandom(1023, C_ID_C, 1, CUSTOMERS_PER_DISTRICT);
}

inline int GetRandomItemID() {
    return NonUniformRandom(8191, OL_I_ID_C, 1, ITEM_COUNT);
}

constexpr const char* const NameTokens[] = {"BAR", "OUGHT", "ABLE", "PRI",
        "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"};

inline std::string GetLastName(int num) {
    std::string result;
    result += NameTokens[num / 100];
    result += NameTokens[(num / 10) % 10];
    result += NameTokens[num % 10];
    return result;
}

inline std::string GetNonUniformRandomLastNameForRun() {
    return GetLastName(NonUniformRandom(255, C_LAST_RUN_C, 0, 999));
}

inline std::string GetNonUniformRandomLastNameForLoad() {
    return GetLastName(NonUniformRandom(255, C_LAST_LOAD_C, 0, 999));
}

//-----------------------------------------------------------------------------

std::string GetFormattedSize(size_t size);

//-----------------------------------------------------------------------------

std::stop_source& GetGlobalInterruptSource();
std::atomic<bool>& GetGlobalErrorVariable();

inline void RequestStopWithError() {
    GetGlobalErrorVariable().store(true);
    GetGlobalInterruptSource().request_stop();
}

//-----------------------------------------------------------------------------

size_t NumberOfMyCpus();

//-----------------------------------------------------------------------------

// Returns the effective PostgreSQL schema name: the given path if non-empty, "public" otherwise.
std::string GetEffectiveSchema(const std::string& path);

} // namespace NTPCC
