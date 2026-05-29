#pragma once
#include <__coroutine/coroutine_handle.h>

namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void __coro_destroy(std::coroutine_handle<> __coro) noexcept {
    try {
        __builtin_coro_destroy(__coro.address());
    } catch (...) { __builtin_unreachable(); }
}
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline auto __coro_destroy_and_resume(
    std::coroutine_handle<> __destroy,                  //
    std::coroutine_handle<> __continue) noexcept        //
    -> std::coroutine_handle<> {
    __coro_destroy(__destroy);
    return __continue;
}
} // namespace __ngr::inline __v0::__core