#pragma once

#include <__ngr/__core/__intrusive_queue.hpp>
#include <__ngr/__core/__memory_mapped_region.hpp>
#include <__ngr/__core/__round_up.hpp>
#include <__ngr/__core/__throw_error_code_if.hpp>

#include <__vector/vector.h>
#include <asm-generic/types.h>
#include <sys/uio.h>

namespace __ngr::inline __v0::__protocal::__memrc {
struct alignas(__GCC_DESTRUCTIVE_SIZE) __descriptor {
    __u32         __bid_; // TODO: false sharing Move to
    __u32         __sc_;  // TODO: false sharing !!
    __s32         __refcount_ = 1;
    char          __pad_[4]{};
    __descriptor *__prev_ = nullptr;
    __descriptor *__next_ = nullptr;
    char          __reserved_[32]{};
    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline void _M_refcnt_increment() noexcept {
        __atomic_add_fetch(&__refcount_, 1, __ATOMIC_RELAXED);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto _M_refcnt_decrement() noexcept -> __s32 {
        return __atomic_add_fetch(&__refcount_, -1, __ATOMIC_ACQ_REL);
    }
    using __queue [[__gnu__::__nodebug__]] =
        __core::__intrusive_queue<&__descriptor::__next_>;
};

struct __block_sc_meta {
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    #error "__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__"
#endif
    __u64 __len_       : 22;
    __u64 __cnt_       : 7;
    __u64 __threshold_ : 6;
    __u64 __batch_     : 5;
    __u64 __reversed_  : 24 = 0;
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]] //
    inline constexpr auto _M_block_n_byte() noexcept -> __u32 {
        __u32 __m = __len_ + sizeof(__descriptor);
        return __core::__round_up(__m * __cnt_, 4096U);
    }
} __attribute__((__trivial_abi__));

inline constexpr __block_sc_meta __k_block_sc_meta[]{
    {   4096, 64, 32, 16}, // 0   4 KB
    {   8192, 32, 16,  8}, // 1   8 KB
    {  16384, 16,  8,  4}, // 2  16 KB
    {  32768, 16,  8,  4}, // 3  32 KB
    {  65536,  8,  4,  2}, // 4  64 KB
    { 131072,  8,  4,  2}, // 5 128 KB
    { 262144,  4,  2,  1}, // 6 256 KB
    { 524288,  4,  2,  1}, // 7 512 KB
    {1048576,  2,  2,  1}, // 8   1 MB
    {2097152,  1,  1,  1}, // 9   2 MB
};

static inline constexpr auto __choose_blksc(__u32 size) noexcept -> __u32 {
    return 19 - __builtin_clz(size);
}

struct __block {
    __u32                          __sc_;
    __core::__memory_mapped_region __mmr_;
    __descriptor::__queue          __free_{};

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline __block(__u32 __r, __block_sc_meta __m, __u32 __bid) noexcept
        : __sc_(__r), __mmr_(__map_region(__m)) {
        const __u32 __k    = __GCC_DESTRUCTIVE_SIZE + __m.__len_;
        char       *__base = static_cast<char *>(__mmr_.__data_);
        for (__u32 __n{}; __n < __m.__cnt_; ++__n) {
            auto *__ptr = ::new (__base + __n * __k + __m.__len_)
                __descriptor{.__bid_ = __bid + __n, .__sc_ = __r};
            __free_._M_push_back(__ptr);
        }
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto __map_region(__block_sc_meta __m) noexcept
        -> __core::__memory_mapped_region {
        int __flags = MAP_PRIVATE | MAP_ANONYMOUS;
        if (__m.__cnt_ == 1) { __flags |= MAP_HUGETLB; }
        __u32 __n   = __m._M_block_n_byte();
        void *__ptr = ::mmap(nullptr, __n, PROT_READ | PROT_WRITE, __flags, -1, 0);
        __core::__throw_error_code_if(__ptr == MAP_FAILED, errno);
        return __core::__memory_mapped_region{__ptr, __n};
    }
};

struct __pool {
    __descriptor::__queue __free_[10];
    std::vector<__block>  __block_;
    __u32                 __next_bid_         = 0;
    bool                  __fixed_registered_ = false;
    ::iovec               __reg_iov_[4096];
    __descriptor         *__bid_to_block_[4096];
};

struct __cache {
    __pool               *__pool_ = nullptr;
    __descriptor::__queue __free_[10];
};

struct __reference {};
} // namespace __ngr::inline __v0::__protocal::__memrc
