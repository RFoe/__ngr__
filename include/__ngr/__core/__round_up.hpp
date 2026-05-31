#pragma once

#include <__cstddef/size_t.h>

namespace __ngr::inline __v0::__core {

[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline constexpr auto
_S_round_up(const std::size_t __x, const std::size_t __a) noexcept
    -> std::size_t {
    return (__x + __a - 1) & ~(__a - 1);
}

[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline auto
_S_round_up(const char *__restrict__ __ptr, const std::size_t __a) noexcept
    -> char * {
    auto __v = reinterpret_cast<__UINTPTR_TYPE__>(__ptr);
    __v      = (__v + __a - 1) & ~(__a - 1);
    return reinterpret_cast<char *>(__v); // NOLINT
}

} // namespace __ngr::inline __v0::__core
