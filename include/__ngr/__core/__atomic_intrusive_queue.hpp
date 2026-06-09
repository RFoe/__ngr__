#pragma once

#include <__atomic/atomic.h>
#include <__ngr/__core/__intrusive_queue.hpp>

namespace __ngr::inline __v0::__core {
template <auto _Next> struct __atomic_intrusive_queue;
template <typename _Tp, _Tp *_Tp::*_Next> struct __atomic_intrusive_queue<_Next> {
    using __pointer        = _Tp *;
    using __atomic_pointer = std::atomic<_Tp *>;

    [[__nodiscard__]]
    auto _M_empty() const noexcept -> bool {
        return __head_.load(std::memory_order_relaxed) == nullptr;
    }

    auto _M_push_front(__pointer __p) noexcept -> bool {
        __pointer __old_head = __head_.load(std::memory_order_relaxed);
        do {
            __p->*_Next = __old_head;
        } while (
            !__head_.compare_exchange_weak(__old_head, __p, std::memory_order_acq_rel));
        return __old_head == nullptr;
    }

    void _M_prepend(__intrusive_queue<_Next> __queue) noexcept {
        __pointer __new_head = __queue._M_front();
        __pointer __tail     = __queue._M_back();
        __pointer __old_head = __head_.load(std::memory_order_relaxed);
        __tail->*_Next       = __old_head;
        while (!__head_.compare_exchange_weak(
            __old_head, __new_head, std::memory_order_acq_rel)) {
            __tail->*_Next = __old_head;
        }
        __queue._M_clear();
    }

    auto _M_pop_swap() noexcept -> __intrusive_queue<_Next> {
        return __intrusive_queue<_Next>::__create(_M_reset());
    }

    auto _M_pop_swap_reversed() noexcept -> __intrusive_queue<_Next> {
        return __intrusive_queue<_Next>::__create_reversed(_M_reset());
    }

    auto _M_reset() noexcept -> __pointer {
        __pointer __old_head = __head_.load(std::memory_order_relaxed);
        while (!__head_.compare_exchange_weak(
            __old_head, nullptr, std::memory_order_acq_rel)) {
            ;
        }
        return __old_head;
    }

    __atomic_pointer __head_{nullptr};
};

} // namespace __ngr::inline __v0::__core
