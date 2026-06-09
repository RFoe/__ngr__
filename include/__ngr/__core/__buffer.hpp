#pragma once

#include <__ngr/__core/__atomic_intrusive_queue.hpp>
#include <linux/io_uring.h>

namespace __ngr::inline __v0::__core {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"

struct alignas(64) __block_prefix {
    __block_prefix    *__next_;
    std::atomic<__u32> __refcount_;
    __u16              __bid_;
    __u8               __reserved_[50];
    __u8               __start_[];
};

struct alignas(64) __descriptor {
    __atomic_intrusive_queue<&__block_prefix::__next_> __recycle_;
    std::atomic<__s32>                                 __n_in_flight_;
    std::atomic<__u32>                                 __refcount_;
    __u32                                              __count_;
    __u8                                               __reserved_[44];
    __u8                                               __start_[];
};

#pragma clang diagnostic pop
} // namespace __ngr::inline __v0::__core
