#pragma once
#include <__coroutine/coroutine_handle.h>

namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void __coro_resume(std::coroutine_handle<> __coro) noexcept {
    try {
        __builtin_coro_resume(__coro.address());
    } catch (...) { __builtin_unreachable(); }
}
} // namespace __ngr::inline __v0::__core
