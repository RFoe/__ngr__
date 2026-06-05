#pragma once

#include <__ngr/__core/__atomic_intrusive_queue.hpp>
#include <__ngr/__core/__intrusive_queue.hpp>
#include <__ngr/__core/__round_up.hpp>
#include <__ngr/__core/__spin_loop_pause.hpp>
#include <__ngr/__core/__throw_error_code_if.hpp>

#include <asm-generic/types.h>
#include <sys/mman.h>

namespace __ngr::inline __v0::__core::__memrc {

struct alignas(__GCC_DESTRUCTIVE_SIZE) __block {
    __PTRDIFF_TYPE__ __use_count_    = 0;
    __block         *__next_         = nullptr;
    char             __reserved_[48] = {0};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
    char __data_[];
#pragma clang diagnostic pop
};

struct __span {
    __block *__source_;
    __u32    __offset_;
    __u32    __length_;
};

static constexpr __u32 __T_ = 16;                 // local threshold for block count
static constexpr __u32 __N_ = 16;
static constexpr __u32 __M_ = 64;
static constexpr __u32 __K_ = __N_ * __M_ / __T_; // max threads
static constexpr __u32 __V_ = __round_up(4096 + 4160 * __M_, 4096);

// page:   4096
// mmap:   (page | __block) * __M_
// remote: mmap * 16
struct __remote_source {
    std::atomic<__u32> __n_ = 0;
    void              *__mmap_[__N_]{};

    struct __atomic_block_queue {
        __atomic_intrusive_queue<&__block::__next_> __blk_{};
        std::atomic<__u32>                          __n_ = 0;
    };
    alignas(__GCC_DESTRUCTIVE_SIZE) __atomic_block_queue __queue_[__K_]{};

    struct __descriptor {
        __u32 __head_ = 0;
        __u32 __tail_ = 0;
    };

    alignas(__GCC_DESTRUCTIVE_SIZE) std::atomic<__u64> __state_{};

    static constexpr __u64 __head_stride_ = 1U;
    static constexpr __u64 __tail_stride_ = 1ULL << 32U;

    ~__remote_source() noexcept {
        for (__u32 __i{}; __i < __n_; ++__i) { ::munmap(__mmap_[__i], __V_); }
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __gnu__::__const__]] //
    inline static auto __convert(__u64 __m) noexcept -> __descriptor {
        return {static_cast<__u32>(__m), static_cast<__u32>(__m >> 32U)};
    }

    auto _M_try_pop_swap_reversed() noexcept -> __intrusive_queue<&__block::__next_> {
        __u64 __u = __state_.load(std::memory_order_acquire);
        while (true) {
            __descriptor __x = __convert(__u);
            if (__x.__head_ == __x.__tail_) [[__unlikely__]] { return {}; }
            if (!__state_.compare_exchange_weak(
                    __u, __u + __tail_stride_, std::memory_order_acq_rel,
                    std::memory_order_acquire)) [[__unlikely__]] {
                continue;
            }
            __atomic_block_queue                &__q = __queue_[__x.__tail_ & (__K_ - 1)];
            __intrusive_queue<&__block::__next_> __r = __q.__blk_._M_pop_swap_reversed();
            __q.__n_.store(0, std::memory_order_release);
            return __r;
        }
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline void _M_allocate() noexcept {
        void *__p = ::mmap(
            nullptr, __V_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        __throw_error_code_if(__p == MAP_FAILED, errno);
        __mmap_[__n_.fetch_add(1, std::memory_order_relaxed)] = __p;

        __intrusive_queue<&__block::__next_> __tmp;
        __block                             *__ptr[__M_];
        char                                *__addr = static_cast<char *>(__p) + 4096;
#pragma unroll
#pragma clang loop vectorize(enable)
        for (__u32 __j{}; __j < __M_; ++__j) {
            __ptr[__j] = ::new (__addr) __block{};
            __tmp._M_push_back(__ptr[__j]);
            __addr += 4160;
        }
        _M_prepend(std::move(__tmp), __ptr);
    }
    auto _M_pop_swap_reversed() noexcept -> __intrusive_queue<&__block::__next_> {
        constexpr __u32 __spin_threshold_ = 16;
        __u32           __spin            = 0;
        while (true) {
            __intrusive_queue<&__block::__next_> __r = _M_try_pop_swap_reversed();
            if (!__r._M_empty()) [[__likely__]] { return __r; }
            if (__spin < __spin_threshold_) [[__likely__]] {
                ++__spin;
                __spin_loop_pause();
            } else {
                _M_allocate();
                __spin = 0;
            }
        }
    }

    void _M_push(__block *__restrict__ __blk) noexcept {
        while (true) {
            __u64                 __u = __state_.load(std::memory_order_acquire);
            __descriptor          __x = __convert(__u);
            __atomic_block_queue &__q = __queue_[__x.__head_ & (__K_ - 1)];
            __u32                 __n = __q.__n_.load(std::memory_order_acquire);

            if (__n == __T_) [[__unlikely__]] { continue; }
            while (!__q.__n_.compare_exchange_weak(
                __n, __n + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                if (__n == __T_) [[__unlikely__]] {
                    __spin_loop_pause();
                    continue;
                }
            }
            __q.__blk_._M_push_front(__blk);
            if (__n + 1 == __T_) [[__unlikely__]] {
                __state_.fetch_add(__head_stride_, std::memory_order_release);
            }
            return;
        }
    }

    void _M_prepend(
        __intrusive_queue<&__block::__next_> __blk, __block *(&__ptr)[__M_]) noexcept {
        __u32 __k = 0;
        while (!__blk._M_empty()) {
            __u64                 __u = __state_.load(std::memory_order_acquire);
            __descriptor          __x = __convert(__u);
            __atomic_block_queue &__q = __queue_[__x.__head_ & (__K_ - 1)];
            __u32                 __n = __q.__n_.load(std::memory_order_acquire);

            if (__n == __T_) [[__unlikely__]] { continue; }
            while (!__q.__n_.compare_exchange_weak(
                __n, __T_, std::memory_order_acq_rel, std::memory_order_acquire)) {
                if (__n == __T_) [[__unlikely__]] {
                    __spin_loop_pause();
                    continue;
                }
            }
            using __iterator    = __intrusive_queue<&__block::__next_>::__iterator;
            __u32    __r        = __T_ - __n;
            __block *__sentinel = (__k + __r < __M_) ? __ptr[__k + __r] : nullptr;
            __intrusive_queue<&__block::__next_> __tmp;
            __tmp._M_splice(
                __tmp.end(), __blk, __blk.begin(),
                __iterator{__ptr[__k + __r - 1], __sentinel});

            __q.__blk_._M_prepend(std::move(__tmp));
            __state_.fetch_add(__head_stride_, std::memory_order_release);
            __k += __r;
        }
    }
} __attribute__((__aligned__(__GCC_DESTRUCTIVE_SIZE)));

inline thread_local __remote_source __k_remote{};

struct __local_source {
    __intrusive_queue<&__block::__next_> __queue_{};
    __block                             *__consume_ = nullptr;
    __u32                                __bump_    = 4096;
    __u32                                __count_   = 0;

    ~__local_source() noexcept {
        if (__consume_ != nullptr) {
            __atomic_fetch_add(&__consume_->__use_count_, -1, __ATOMIC_ACQ_REL);
        }
        __queue_._M_clear();
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline void _M_allocate() noexcept {
        if (__consume_ != nullptr &&
            __atomic_fetch_add(&__consume_->__use_count_, -1, __ATOMIC_ACQ_REL) == 1) {
            _M_release(__consume_);
        }
        if (__queue_._M_empty()) [[__unlikely__]] {
            __queue_._M_prepend(__k_remote._M_pop_swap_reversed());
            __count_ += __T_;
        }
        __consume_ = __queue_._M_pop_front();
        __bump_    = 0;
        __atomic_fetch_add(&__consume_->__use_count_, 1, __ATOMIC_RELAXED);
        --__count_;
    }

    auto _M_acquire(__u32 __n) noexcept -> __span {
        if (4096 - __bump_ < __n) [[__unlikely__]] { _M_allocate(); }
        __atomic_fetch_add(&__consume_->__use_count_, 1, __ATOMIC_RELAXED);
        return __span{
            .__source_ = __consume_,
            .__offset_ = std::exchange(__bump_, __bump_ + __n),
            .__length_ = __n};
    };

    void _M_release(__block *__restrict__ __blk) noexcept {
        if (__count_ < __T_) [[__likely__]] {
            __queue_._M_push_front(__blk);
            ++__count_;
            return;
        }
        __k_remote._M_push(__blk);
    }
} __attribute__((__aligned__(__GCC_DESTRUCTIVE_SIZE)));

inline thread_local __local_source __k_local;

[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline auto __acquire(__u32 __n) noexcept -> __span {
    return __k_local._M_acquire(__n);
}

[[__gnu__::__always_inline__, __gnu__::__artificial__]] //
inline void __release(__span const &__x) noexcept {
    if (__atomic_fetch_add(&__x.__source_->__use_count_, -1, __ATOMIC_ACQ_REL) == 1) {
        __k_local._M_release(__x.__source_);
    }
}

} // namespace __ngr::inline __v0::__core::__memrc

namespace __ngr::inline __v0::__core {

using __memrc::__span __attribute__((__using_if_exists__));
using __memrc::__release __attribute__((__using_if_exists__));
using __memrc::__acquire __attribute__((__using_if_exists__));

} // namespace __ngr::inline __v0::__core
