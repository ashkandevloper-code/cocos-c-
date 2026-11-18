// Bit-exact port of the Dart VM's dart:math Random(seed) (multiply-with-carry, Wang seed mix).
// Online matches are seeded server-side and replayed on every phone, so the C++ client must draw
// exactly the same numbers as the Flutter client. Verified against Dart 3.x in tests/test_rng.cpp.
#pragma once
#include <cstdint>
#include <stdexcept>

namespace nx {

class DartRandom {
public:
    explicit DartRandom(int64_t seed) {
        uint64_t s = setupSeed(static_cast<uint64_t>(seed));
        lo_ = static_cast<uint32_t>(s & 0xffffffffu);
        hi_ = static_cast<uint32_t>(s >> 32);
        nextState(); nextState(); nextState(); nextState();
    }
    /// Unseeded: Dart picks a random seed; we use a time-ish value (non-deterministic by design).
    static DartRandom unseeded();

    int nextInt(int max) {
        if (max <= 0) throw std::invalid_argument("nextInt max must be > 0");
        nextState();
        if ((max & -max) == max) return static_cast<int>(lo_ & static_cast<uint32_t>(max - 1));
        uint64_t rnd32, result;
        do {
            rnd32 = lo_;
            result = rnd32 % static_cast<uint64_t>(max);
        } while ((rnd32 - result + static_cast<uint64_t>(max)) > (uint64_t(1) << 32));
        return static_cast<int>(result);
    }
    double nextDouble() {
        const double p27 = double(1 << 27), p53 = 9007199254740992.0;
        double a = nextInt(1 << 26);
        double b = nextInt(1 << 27);
        return (a * p27 + b) / p53;
    }
    bool nextBool() { return nextInt(2) == 0; }

private:
    uint32_t lo_ = 0, hi_ = 0;
    void nextState() {
        const uint64_t A = 0xffffda61ull;
        uint64_t st = A * lo_ + hi_;
        lo_ = static_cast<uint32_t>(st & 0xffffffffu);
        hi_ = static_cast<uint32_t>(st >> 32);
    }
    static uint64_t setupSeed(uint64_t seed) {
        // Thomas Wang 64-bit mix, as in the Dart VM (logical shifts, wrapping arithmetic).
        seed = (~seed) + (seed << 21);
        seed = seed ^ (seed >> 24);
        seed = (seed + (seed << 3)) + (seed << 8);
        seed = seed ^ (seed >> 14);
        seed = (seed + (seed << 2)) + (seed << 4);
        seed = seed ^ (seed >> 28);
        seed = seed + (seed << 31);
        if (seed == 0) seed = 0x5a17;
        return seed;
    }
};

}  // namespace nx
