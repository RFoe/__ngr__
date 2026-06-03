#pragma once

#include <asm-generic/types.h>

#include <memory>

namespace __ngr::inline __v0::__protocal::__memrc {
struct __pool;
struct alignas(__GCC_DESTRUCTIVE_SIZE) __prefix {
    __u32     __bid_;
    __u32     __sc_;
    __s32     __refcount_ = 1;
    char      __pad_[32];
    __prefix *__next_ = nullptr;
};

struct __mem_sc_meta {
    __u32 __to_mmap_;
    __u32 __n_chunk_;
    __u32 __threshold_;
    __u32 __batch_;
    constexpr __mem_sc_meta(__u32 __m, __u32 __n, __u32 __k, __u32 __x) noexcept
        : __to_mmap_{__to_mmap_prefix(__m, __n)}, __n_chunk_{__n},
          __threshold_{__k}, __batch_{__x} {}
    static constexpr auto __to_mmap_prefix(__u32 __m, __u32 __n) noexcept
        -> __u32 {
        constexpr size_t prefix_sz = sizeof(__prefix);
        constexpr __u32  page_size = 4096;

        size_t per_chunk = static_cast<size_t>(__m) + prefix_sz;

        size_t total = per_chunk * __n;

        // 向上对齐到页大小
        return static_cast<__u32>(
            (total + page_size - 1) / page_size * page_size);
    }
};

inline constexpr __mem_sc_meta __k_mem_sc_table[]{
    {   4096, 64, 32, 16}, // 0   4 KB
    {   8192, 32, 16,  8}, // 1   8 KB
    {  16384, 16,  8,  4}, // 2  16 KB
    {  32768, 16,  8,  4}, // 3  32 KB
    {  65536,  8,  4,  2}, // 4  64 KB
    { 131072,  8,  4,  2}, // 5 128 KB
    { 262144,  4,  2,  1}, // 6 256 KB
    { 524288,  4,  2,  1}, // 7 512 KB
    {1048576,  2,  2,  1}, // 8   1 MB
    {2097152,  1,  1,  1}, // 9   2 MB  (MAP_HUGETLB)
};

inline constexpr __u32 __k_n_mem_sc =
    sizeof __k_mem_sc_table / sizeof __k_mem_sc_table[0];

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto __buf_sc_of(const __u32 __sz) noexcept -> int {
    for (__u32 __i = 0; __i < __k_n_mem_sc; ++__i) {
        if (__k_mem_sc_table[__i].__to_mmap_ >= __sz) { return __i; }
    }
    return -1;
}

void func() noexcept {
    std::shared_ptr<int> s = std::make_shared<int>();
    s.reset();
}

} // namespace __ngr::inline __v0::__protocal::__memrc