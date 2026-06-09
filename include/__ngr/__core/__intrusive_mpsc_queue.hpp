#pragma once

#include <__atomic/atomic.h>
#include <__concepts/constructible.h>
#include <cassert>

namespace __ngr::inline __v0::__core {
template <auto _Ptr> struct __intrusive_mpsc_queue;

template <std::default_initializable _Node, std::atomic<_Node *> _Node::*_Next>
struct __intrusive_mpsc_queue<_Next> {
    std::atomic<_Node *> __head_{&__stub_};
    _Node               *__tail_{&__stub_};
    _Node                __stub_{};

    __intrusive_mpsc_queue() {
        (__stub_.*_Next).store(nullptr, std::memory_order_release);
    }

    constexpr auto _M_push_back(_Node *__new_node) noexcept -> bool {
        (__new_node->*_Next).store(nullptr, std::memory_order_relaxed);
        _Node *__prev = __head_.exchange(__new_node, std::memory_order_acq_rel);
        (__prev->*_Next).store(__new_node, std::memory_order_release);
        return __prev == &__stub_;
    }

    constexpr auto _M_pop_front() noexcept -> _Node * {
        _Node *__tail = this->__tail_;
        assert(__tail != nullptr);
        _Node *__next = (__tail->*_Next).load(std::memory_order_acquire);
        // If tail is pointing to the stub node we need to advance it once more
        if (&__stub_ == __tail) {
            if (nullptr == __next) { return nullptr; }
            this->__tail_ = __next;
            __tail        = __next;
            __next        = (__next->*_Next).load(std::memory_order_acquire);
        }
        // Normal case: there is a next node and we can just advance the tail
        if (nullptr != __next) {
            this->__tail_ = __next;
            return __tail;
        }
        // Next is nullptr here means that either:
        // 1) There are no more nodes in the queue
        // 2) A producer is in the middle of adding a new node
        _Node const *__head = this->__head_.load(std::memory_order_acquire);
        // A producer is in the middle of adding a new node
        // we cannot return tail as we cannot link the next node yet
        if (__tail != __head) { return nullptr; }
        // No more nodes in the queue - we need to insert a stub node
        // to be able to link to an eventual empty state (or new nodes)
        _M_push_back(&__stub_);
        // Now re-attempt to load next
        __next = (__tail->*_Next).load(std::memory_order_acquire);
        if (nullptr != __next) {
            // Successfully linked either a new node or the stub node
            this->__tail_ = __next;
            return __tail;
        }
        // A producer is in the middle of adding a new node since next is still nullptr
        // and not our stub node, thus we cannot link the next node yet
        return nullptr;
    }
};

} // namespace __ngr::inline __v0::__core
