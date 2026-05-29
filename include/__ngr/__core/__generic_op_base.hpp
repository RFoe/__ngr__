#pragma once
#include <__ngr/__core/__intrusive_mpsc_queue.hpp>

namespace __ngr::inline __v0::__core {
struct __once_flag {
    std::atomic<bool> __flag_ = false;

    [[__nodiscard__]] auto _M_try_race() noexcept -> bool {
        return !__flag_.exchange(true, std::memory_order_acq_rel);
    }
};
struct __generic_op_base {
    enum class _Op : unsigned char { _S_submit, _S_stop };
    __once_flag                      __flag_;
    _Op                              __op_;
    std::atomic<__generic_op_base *> __next_;
};
struct __generic_op_queue {
    __intrusive_mpsc_queue<&__generic_op_base::__next_> __queue_;
};

} // namespace __ngr::inline __v0::__core
