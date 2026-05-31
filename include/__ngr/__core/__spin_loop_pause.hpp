#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) ||             \
    defined(_M_IX86)
    #if defined(_MSC_VER) || (defined(__EDG__) && defined(__INTELLISENSE__))
        #include <intrin.h>
    #endif
namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void _S_spin_loop_pause() noexcept {
    #if defined(_MSC_VER) || (defined(__EDG__) && defined(__INTELLISENSE__))
    _mm_pause();
    #else
    __builtin_ia32_pause();
    #endif
}
} // namespace __ngr::inline __v0::__core
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void _S_spin_loop_pause() noexcept {
    #if (                                                                      \
        defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) ||                \
        defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) ||              \
        defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) ||                 \
        defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) ||                \
        defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_8A__) ||                \
        defined(__aarch64__))
    asm volatile("yield" ::: "memory");
    #elif defined(_M_ARM64)
    __yield();
    #else
    asm volatile("nop" ::: "memory");
    #endif
}
} // namespace __ngr::inline __v0::__core
#else
namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void _S_spin_loop_pause() noexcept {}
} // namespace __ngr::inline __v0::__core
#endif
