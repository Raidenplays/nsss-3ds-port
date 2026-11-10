#pragma once
// Bit-exact port of java.util.Random. Beta's world generation depends on this
// producing the same numbers as Java, so the same seed gives the same world.
#include <cmath>
#include <cstdint>

class JavaRandom {
public:
    explicit JavaRandom(int64_t seed = 0) { setSeed(seed); }

    void setSeed(int64_t s) {
        seed = (static_cast<uint64_t>(s) ^ 0x5DEECE66DULL) & MASK;
        haveNextNextGaussian = false;
    }

    uint64_t state() const { return seed; } // the internal 48-bit seed (debugging)
    void setState(uint64_t s) { seed = s & MASK; }
    int32_t nextInt() { return next(32); }

    int32_t nextInt(int32_t bound) {
        if (bound <= 0) return 0; // Java throws; we just guard
        if ((bound & -bound) == bound) // power of two
            return static_cast<int32_t>((static_cast<int64_t>(bound) * static_cast<int64_t>(next(31))) >> 31);
        int32_t bits, val;
        do {
            bits = next(31);
            val = bits % bound;
        } while (static_cast<int32_t>(static_cast<uint32_t>(bits) - static_cast<uint32_t>(val) +
                                      static_cast<uint32_t>(bound - 1)) < 0);
        return val;
    }

    int64_t nextLong() {
        return static_cast<int64_t>((static_cast<uint64_t>(static_cast<int64_t>(next(32))) << 32) +
                                    static_cast<uint64_t>(static_cast<int64_t>(next(32))));
    }

    bool nextBoolean() { return next(1) != 0; }

    float nextFloat() { return next(24) / static_cast<float>(1 << 24); }

    double nextDouble() {
        return static_cast<double>((static_cast<int64_t>(next(26)) << 27) + next(27)) *
               (1.0 / static_cast<double>(1LL << 53));
    }

    double nextGaussian() {
        if (haveNextNextGaussian) {
            haveNextNextGaussian = false;
            return nextNextGaussian;
        }
        double v1, v2, s;
        do {
            v1 = 2 * nextDouble() - 1;
            v2 = 2 * nextDouble() - 1;
            s = v1 * v1 + v2 * v2;
        } while (s >= 1 || s == 0);
        double mul = std::sqrt(-2 * std::log(s) / s);
        nextNextGaussian = v2 * mul;
        haveNextNextGaussian = true;
        return v1 * mul;
    }

private:
    double nextNextGaussian = 0;
    static constexpr uint64_t MULT = 0x5DEECE66DULL;
    static constexpr uint64_t MASK = (1ULL << 48) - 1;
    uint64_t seed;
    bool haveNextNextGaussian = false;

    int32_t next(int bits) {
        seed = (seed * MULT + 0xBULL) & MASK;
        // Java: (int)(seed >>> (48 - bits)), sign comes from the int cast
        return static_cast<int32_t>(static_cast<uint32_t>(seed >> (48 - bits)));
    }
};
