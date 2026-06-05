#pragma once

#include <__cstddef/ptrdiff_t.h>
#include <__utility/exchange.h>
#include <__utility/swap.h>
#include <cassert>

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {
template <auto _Next> struct __intrusive_queue;

template <typename _Ty, _Ty *_Ty::*_Next> struct __intrusive_queue<_Next> {
    struct __iterator;
    using iterator = __iterator;

    constexpr __intrusive_queue() noexcept = default;
    constexpr __intrusive_queue(__intrusive_queue &&__other) noexcept
        : __head_(std::exchange(__other.__head_, nullptr)),
          __tail_(std::exchange(__other.__tail_, nullptr)) {}
    constexpr __intrusive_queue(_Ty *__head, _Ty *__tail) noexcept
        : __head_(__head), __tail_(__tail) {}

    constexpr auto operator=(__intrusive_queue __other) noexcept -> __intrusive_queue & {
        std::swap(__head_, __other.__head_);
        std::swap(__tail_, __other.__tail_);
        return *this;
    }
    constexpr ~__intrusive_queue() { assert(_M_empty()); }

    [[__nodiscard__]]
    constexpr auto _M_empty() const noexcept -> bool {
        return __head_ == nullptr;
    }

    constexpr void _M_clear() noexcept {
        __head_ = nullptr;
        __tail_ = nullptr;
    }

    [[__nodiscard__]]
    constexpr auto _M_pop_front() noexcept -> _Ty * {
        if (_M_empty()) { return nullptr; }
        _Ty *__item = std::exchange(__head_, __head_->*_Next);
        if (__item->*_Next == nullptr) { __tail_ = nullptr; }
        return __item;
    }

    constexpr void _M_push_front(_Ty *__item) noexcept {
        assert(__item != nullptr);
        __item->*_Next = __head_;
        __head_        = __item;
        if (__tail_ == nullptr) { __tail_ = __item; }
    }

    constexpr void _M_push_back(_Ty *__item) noexcept {
        assert(__item != nullptr);
        __item->*_Next = nullptr;
        if (__tail_ == nullptr) {
            __head_ = __item;
        } else {
            __tail_->*_Next = __item;
        }
        __tail_ = __item;
    }

    constexpr void _M_append(__intrusive_queue __other) noexcept {
        if (__other._M_empty()) return;
        auto *__other_head = std::exchange(__other.__head_, nullptr);
        if (_M_empty()) {
            __head_ = __other_head;
        } else {
            __tail_->*_Next = __other_head;
        }
        __tail_ = std::exchange(__other.__tail_, nullptr);
    }

    constexpr void _M_prepend(__intrusive_queue __other) noexcept {
        if (__other._M_empty()) return;

        __other.__tail_->*_Next = __head_;
        __head_                 = __other.__head_;
        if (__tail_ == nullptr) { __tail_ = __other.__tail_; }

        __other.__tail_ = nullptr;
        __other.__head_ = nullptr;
    }

    static constexpr auto __create_reversed(_Ty *__list) noexcept -> __intrusive_queue {
        _Ty *__new_head = nullptr;
        _Ty *__new_tail = __list;
        while (__list != nullptr) {
            _Ty *__next    = __list->*_Next;
            __list->*_Next = __new_head;
            __new_head     = __list;
            __list         = __next;
        }

        __intrusive_queue __result;
        __result.__head_ = __new_head;
        __result.__tail_ = __new_tail;
        return __result;
    }

    static constexpr auto __create(_Ty *__list) noexcept -> __intrusive_queue {
        __intrusive_queue __result{};
        __result.__head_ = __list;
        __result.__tail_ = __list;
        if (__list == nullptr) { return __result; }
        while (__result.__tail_->*_Next != nullptr) {
            __result.__tail_ = __result.__tail_->*_Next;
        }
        return __result;
    }

    struct __iterator {
        using difference_type = std::ptrdiff_t;
        using value_type      = _Ty *;

        _Ty *__predecessor_ = nullptr;
        _Ty *__item_        = nullptr;

        constexpr __iterator() noexcept = default;

        constexpr explicit __iterator(_Ty *__pred, _Ty *__item) noexcept
            : __predecessor_(__pred), __item_(__item) {}

        [[__nodiscard__]]
        constexpr auto operator*() const noexcept -> _Ty * {
            assert(__item_ != nullptr);
            return __item_;
        }

        [[__nodiscard__]]
        constexpr auto operator->() const noexcept -> _Ty ** {
            assert(__item_ != nullptr);
            return &__item_;
        }

        constexpr auto operator++() noexcept -> __iterator & {
            __predecessor_ = __item_;
            if (__item_) { __item_ = __item_->*_Next; }
            return *this;
        }

        constexpr auto operator++(int) noexcept -> __iterator {
            __iterator __result = *this;
            ++*this;
            return __result;
        }

        friend constexpr auto operator==(__iterator const &, __iterator const &) noexcept
            -> bool = default;
    };

    [[__nodiscard__]]
    constexpr auto begin() const noexcept -> __iterator {
        return __iterator(nullptr, __head_);
    }

    [[__nodiscard__]]
    constexpr auto end() const noexcept -> __iterator {
        return __iterator(__tail_, nullptr);
    }

    constexpr void _M_splice(
        __iterator __pos, __intrusive_queue &__other, __iterator __first,
        __iterator __last) noexcept {
        if (__first == __last) { return; }
        assert(__first.__item_ != nullptr);
        assert(__last.__predecessor_ != nullptr);
        if (__other.__head_ == __first.__item_) {
            __other.__head_ = __last.__item_;
            if (__other.__head_ == nullptr) { __other.__tail_ = nullptr; }
        } else {
            assert(__first.__predecessor_ != nullptr);
            __first.__predecessor_->*_Next = __last.__item_;
            __last.__predecessor_->*_Next  = __pos.__item_;
        }
        if (_M_empty()) {
            __head_ = __first.__item_;
            __tail_ = __last.__predecessor_;
        } else {
            __pos.__predecessor_->*_Next = __first.__item_;
            if (__pos.__item_ == nullptr) { __tail_ = __last.__predecessor_; }
        }
    }

    constexpr auto _M_front() const noexcept -> _Ty * { return __head_; }
    constexpr auto _M_back() const noexcept -> _Ty * { return __tail_; }

    _Ty *__head_ = nullptr;
    _Ty *__tail_ = nullptr;
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)
