#pragma once
#include <__atomic/atomic.h>

namespace __ngr::inline __v0::__core {
struct __once_flag {
    std::atomic<bool> __flag_ = false;

    [[__nodiscard__]] auto _M_try() noexcept -> bool {
        return !__flag_.exchange(true, std::memory_order_acq_rel);
    }
};
} // namespace __ngr::inline __v0::__core
