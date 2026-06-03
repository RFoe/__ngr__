#pragma once

#include <__algorithm/min.h>
#include <__atomic/atomic.h>
#include <__bit/bit_width.h>
#include <__new/global_new_delete.h>
#include <__numeric/accumulate.h>
#include <linux/membarrier.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace __ngr::inline __v0::__core {

inline void __attribute__((__constructor__))
_SYSCALL_MEMBARRIER_REGISTER() noexcept {
    ::syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0);
}

template <std::default_initializable _Ty, std::size_t __max_>
struct __atomic_segment_array {
    using __atomic = std::atomic<_Ty>;

    std::atomic<__atomic *> __seg_[std::bit_width(__max_)]{};
    std::atomic<unsigned>   __count_                              = 0;
    alignas(__GCC_DESTRUCTIVE_SIZE) std::atomic<unsigned> __prev_ = 0;

    ~__atomic_segment_array() noexcept {
        for (auto &__p : __seg_) {
            delete[] __p.load(std::memory_order_relaxed);
        }
    }

    struct __locate_result {
        unsigned __seg_;
        unsigned __pos_;
    };

    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static constexpr auto __locate(unsigned __w) noexcept
        -> __locate_result {
        unsigned __n = 63U - __builtin_clzll(__w + 1U);
        return {__n, __w - ((1U << __n) - 1U)};
    }

    [[__nodiscard__]] auto _M_segment(unsigned __n) noexcept -> __atomic * {
        __atomic *__seg = __seg_[__n].load(std::memory_order_acquire);
        if (__seg != nullptr) [[__likely__]] { return __seg; }
        auto *__tmp = ::new (std::nothrow) __atomic[1U << __n]{};
        if (__seg_[__n].compare_exchange_strong(
                __seg, __tmp, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return __tmp;
        }
        delete[] __tmp;
        return __seg;
    }

    auto _M_push_back(_Ty __v) noexcept -> std::size_t {
        unsigned __prev = __prev_.fetch_add(1, std::memory_order_relaxed);
        auto [__n, __m] = __locate(__prev);
        _M_segment(__n)[__m].store(__v, std::memory_order_relaxed);

        __asm__ __volatile__("" ::: "memory");

        while (__builtin_expect(
            __count_.load(std::memory_order_relaxed) != __prev, 0)) {}
        __count_.store(__prev + 1, std::memory_order_relaxed);

        return __prev;
    }

    template <typename _Uy, typename _Fn>
    auto _M_fold_inplace(_Fn __fn) const noexcept -> _Uy {
        _Uy __init{};
        ::syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0);
        std::size_t __r = __count_.load(std::memory_order_relaxed);
        unsigned    __w{};
        unsigned    __n{};
        while (__w < __r) {
            __atomic   *__seg = __seg_[__n].load(std::memory_order_acquire);
            std::size_t __mm  = std::min(1UL << __n, __r - __w);
#pragma unroll
#pragma clang loop vectorize(enable)
            for (std::size_t __m{}; __m < __mm; ++__m) {
                __fn(__init, __seg[__m].load(std::memory_order_relaxed));
            }
            __w += __mm;
            ++__n;
        }
        return __init;
    }
};

} // namespace __ngr::inline __v0::__core
