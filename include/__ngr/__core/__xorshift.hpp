#pragma once

#include <cstdint>
#include <random>

namespace __ngr::inline __v0::__core {

class xorshift {
  public:
    using result_type = std::uint32_t;

    static constexpr auto(min)() -> result_type { return 0; }

    static constexpr auto(max)() -> result_type { return UINT32_MAX; }

    friend auto operator==(xorshift const &, xorshift const &)
        -> bool = default;

    xorshift() : m_seed(0xc1f651c67c62c6e0ull) {}

    explicit xorshift(std::random_device &rd) { seed(rd); }

    explicit xorshift(std::uint64_t seed) : m_seed(seed) {}

    void seed(std::random_device &rd) {
        m_seed = std::uint64_t(rd()) << 31 | std::uint64_t(rd());
    }

    auto operator()() -> result_type {
        std::uint64_t result = m_seed * 0xd989bcacc137dcd5ull;
        m_seed ^= m_seed >> 11;
        m_seed ^= m_seed << 31;
        m_seed ^= m_seed >> 18;
        return std::uint32_t(result >> 32ull);
    }

    void discard(unsigned long long n) {
        for (unsigned long long i = 0; i < n; ++i) operator()();
    }

  private:
    std::uint64_t m_seed;
};

} // namespace __ngr::inline __v0::__core
