#pragma once

#include <__cstddef/ptrdiff_t.h>
#include <__utility/exchange.h>
#include <__utility/swap.h>
#include <cassert>

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {
template <auto _Next> struct __intrusive_queue;

template <typename _Item, _Item *_Item::*_Next>
struct __intrusive_queue<_Next> {
    constexpr __intrusive_queue() noexcept = default;

    constexpr __intrusive_queue(__intrusive_queue &&__other) noexcept
        : __head_(std::exchange(__other.__head_, nullptr)),
          __tail_(std::exchange(__other.__tail_, nullptr)) {}

    constexpr __intrusive_queue(_Item *__head, _Item *__tail) noexcept
        : __head_(__head), __tail_(__tail) {}

    constexpr auto operator=(__intrusive_queue __other) noexcept
        -> __intrusive_queue & {
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
    constexpr auto _M_pop_front() noexcept -> _Item * {
        if (_M_empty()) { return nullptr; }
        _Item *__item = std::exchange(__head_, __head_->*_Next);
        if (__item->*_Next == nullptr) { __tail_ = nullptr; }
        return __item;
    }

    constexpr void _M_push_front(_Item *__item) noexcept {
        assert(__item != nullptr);
        __item->*_Next = __head_;
        __head_        = __item;
        if (__tail_ == nullptr) { __tail_ = __item; }
    }

    constexpr void _M_push_back(_Item *__item) noexcept {
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

    struct iterator {
        using difference_type = std::ptrdiff_t;
        using value_type      = _Item *;

        _Item *__predecessor_ = nullptr;
        _Item *__item_        = nullptr;

        constexpr iterator() noexcept = default;

        constexpr explicit iterator(_Item *__pred, _Item *__item) noexcept
            : __predecessor_(__pred), __item_(__item) {}

        [[__nodiscard__]]
        constexpr auto operator*() const noexcept -> _Item * {
            assert(__item_ != nullptr);
            return __item_;
        }

        [[__nodiscard__]]
        constexpr auto operator->() const noexcept -> _Item ** {
            assert(__item_ != nullptr);
            return &__item_;
        }

        constexpr auto operator++() noexcept -> iterator & {
            __predecessor_ = __item_;
            if (__item_) { __item_ = __item_->*_Next; }
            return *this;
        }

        constexpr auto operator++(int) noexcept -> iterator {
            iterator __result = *this;
            ++*this;
            return __result;
        }

        friend constexpr auto
        operator==(iterator const &, iterator const &) noexcept
            -> bool = default;
    };

    [[__nodiscard__]]
    constexpr auto begin() const noexcept -> iterator {
        return iterator(nullptr, __head_);
    }

    [[__nodiscard__]]
    constexpr auto end() const noexcept -> iterator {
        return iterator(__tail_, nullptr);
    }

    constexpr void splice(
        iterator pos, __intrusive_queue &other, iterator first,
        iterator last) noexcept {
        if (first == last) { return; }
        assert(first.__item_ != nullptr);
        assert(last.__predecessor_ != nullptr);
        if (other.__head_ == first.__item_) {
            other.__head_ = last.__item_;
            if (other.__head_ == nullptr) { other.__tail_ = nullptr; }
        } else {
            assert(first.__predecessor_ != nullptr);
            first.__predecessor_->*_Next = last.__item_;
            last.__predecessor_->*_Next  = pos.__item_;
        }
        if (_M_empty()) {
            __head_ = first.__item_;
            __tail_ = last.__predecessor_;
        } else {
            pos.__predecessor_->*_Next = first.__item_;
            if (pos.__item_ == nullptr) { __tail_ = last.__predecessor_; }
        }
    }

    constexpr auto _M_front() const noexcept -> _Item * { return __head_; }

    constexpr auto _M_back() const noexcept -> _Item * { return __tail_; }

    _Item *__head_ = nullptr;
    _Item *__tail_ = nullptr;
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)
