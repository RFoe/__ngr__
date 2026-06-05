#pragma once

namespace __ngr::inline __v0::__core {

template <typename _Ty, typename _Uy>
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline constexpr auto __round_up(const _Ty __x, const _Uy __a) noexcept -> _Ty {
    return (__x + __a - 1) & ~(__a - 1);
}

template <typename _Ty> [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline auto __round_up(const void *__restrict__ __ptr, const _Ty __a) noexcept -> char * {
    auto __v = reinterpret_cast<__UINTPTR_TYPE__>(__ptr);
    __v      = (__v + __a - 1) & ~(__a - 1);
    return reinterpret_cast<char *>(__v); // NOLINT
}

} // namespace __ngr::inline __v0::__core
