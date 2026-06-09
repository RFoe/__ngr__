#pragma once
#include <asm-generic/types.h>
namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline constexpr auto __round_up(const __u64 __x, const __u64 __a) noexcept -> __u64 {
    return (__x + __a - 1) & ~(__a - 1);
}
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline auto __round_up(const void *__restrict__ __ptr, const __u64 __a) noexcept
    -> char * {
    auto __v = reinterpret_cast<__UINTPTR_TYPE__>(__ptr);
    __v      = (__v + __a - 1) & ~(__a - 1);
    return reinterpret_cast<char *>(__v); // NOLINT
}
} // namespace __ngr::inline __v0::__core
