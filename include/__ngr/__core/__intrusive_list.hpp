#pragma once

#include <__cstddef/ptrdiff_t.h>
#include <__utility/exchange.h>
#include <__utility/swap.h>
#include <cassert>

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {

template <auto _Prev, auto _Next> struct __intrusive_list;

template <typename _Item, _Item *_Item::*_Prev, _Item *_Item::*_Next>
struct __intrusive_list<_Prev, _Next> {
    constexpr __intrusive_list() noexcept = default;

    constexpr __intrusive_list(__intrusive_list &&__other) noexcept
        : __head_(std::exchange(__other.__head_, nullptr)),
          __tail_(std::exchange(__other.__tail_, nullptr)) {}

    constexpr __intrusive_list(_Item *__head, _Item *__tail) noexcept
        : __head_(__head), __tail_(__tail) {}

    constexpr auto operator=(__intrusive_list __other) noexcept
        -> __intrusive_list & {
        std::swap(__head_, __other.__head_);
        std::swap(__tail_, __other.__tail_);
        return *this;
    }

    constexpr ~__intrusive_list() { assert(_M_empty()); }

    [[__nodiscard__]]
    constexpr auto _M_empty() const noexcept -> bool {
        return __head_ == nullptr;
    }

    constexpr auto _M_front() const noexcept -> _Item * { return __head_; }
    constexpr auto _M_back() const noexcept -> _Item * { return __tail_; }

    constexpr void _M_clear() noexcept {
        __head_ = nullptr;
        __tail_ = nullptr;
    }

    constexpr void _M_push_back(_Item *__item) noexcept {
        assert(__item != nullptr);
        __item->*_Next = nullptr;
        __item->*_Prev = __tail_;
        if (__tail_ == nullptr) {
            __head_ = __item;
        } else {
            __tail_->*_Next = __item;
        }
        __tail_ = __item;
    }

    constexpr void _M_push_front(_Item *__item) noexcept {
        assert(__item != nullptr);
        __item->*_Prev = nullptr;
        __item->*_Next = __head_;
        if (__head_ == nullptr) {
            __tail_ = __item;
        } else {
            __head_->*_Prev = __item;
        }
        __head_ = __item;
    }

    [[__nodiscard__]]
    constexpr auto _M_pop_front() noexcept -> _Item * {
        if (_M_empty()) { return nullptr; }
        _Item *__item = __head_;
        __head_       = __item->*_Next;
        if (__head_ != nullptr) {
            __head_->*_Prev = nullptr;
        } else {
            __tail_ = nullptr;
        }
        __item->*_Next = nullptr;
        __item->*_Prev = nullptr;
        return __item;
    }

    [[__nodiscard__]]
    constexpr auto _M_pop_back() noexcept -> _Item * {
        if (_M_empty()) { return nullptr; }
        _Item *__item = __tail_;
        __tail_       = __item->*_Prev;
        if (__tail_ != nullptr) {
            __tail_->*_Next = nullptr;
        } else {
            __head_ = nullptr;
        }
        __item->*_Next = nullptr;
        __item->*_Prev = nullptr;
        return __item;
    }

    constexpr void _M_erase(_Item *__item) noexcept {
        assert(__item != nullptr);

        _Item *__prev = __item->*_Prev;
        _Item *__next = __item->*_Next;

        if (__prev != nullptr) {
            __prev->*_Next = __next;
        } else {
            assert(__head_ == __item);
            __head_ = __next;
        }

        if (__next != nullptr) {
            __next->*_Prev = __prev;
        } else {
            assert(__tail_ == __item);
            __tail_ = __prev;
        }

        __item->*_Next = nullptr;
        __item->*_Prev = nullptr;
    }

    constexpr void _M_append(__intrusive_list __other) noexcept {
        if (__other._M_empty()) return;
        _Item *__other_head = std::exchange(__other.__head_, nullptr);
        _Item *__other_tail = std::exchange(__other.__tail_, nullptr);

        __other_head->*_Prev = __tail_;
        if (_M_empty()) {
            __head_ = __other_head;
        } else {
            __tail_->*_Next = __other_head;
        }
        __tail_ = __other_tail;
    }

    constexpr void _M_prepend(__intrusive_list __other) noexcept {
        if (__other._M_empty()) return;
        _Item *__other_head = std::exchange(__other.__head_, nullptr);
        _Item *__other_tail = std::exchange(__other.__tail_, nullptr);

        __other_tail->*_Next = __head_;
        if (_M_empty()) {
            __tail_ = __other_tail;
        } else {
            __head_->*_Prev = __other_tail;
        }
        __head_ = __other_head;
    }

    struct __iterator {
        using difference_type = std::ptrdiff_t;
        using value_type      = _Item *;

        _Item *__item_ = nullptr;

        constexpr __iterator() noexcept = default;
        constexpr explicit __iterator(_Item *__item) noexcept
            : __item_(__item) {}

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

        constexpr auto operator++() noexcept -> __iterator & {
            if (__item_) { __item_ = __item_->*_Next; }
            return *this;
        }

        constexpr auto operator++(int) noexcept -> __iterator {
            __iterator __result = *this;
            ++*this;
            return __result;
        }

        constexpr auto operator--() noexcept -> __iterator & {
            if (__item_) { __item_ = __item_->*_Prev; }
            return *this;
        }

        constexpr auto operator--(int) noexcept -> __iterator {
            __iterator __result = *this;
            --*this;
            return __result;
        }

        friend constexpr auto
        operator==(__iterator const &, __iterator const &) noexcept
            -> bool = default;
    };

    [[__nodiscard__]]
    constexpr auto begin() const noexcept -> __iterator {
        return __iterator(__head_);
    }

    [[__nodiscard__]]
    constexpr auto end() const noexcept -> __iterator {
        return __iterator(nullptr);
    }

    static constexpr auto _M_create(_Item *__list) noexcept
        -> __intrusive_list {
        __intrusive_list __result{};
        if (__list == nullptr) return __result;

        __list->*_Prev   = nullptr;
        __result.__head_ = __list;
        while (__list->*_Next != nullptr) {
            __list->*_Next->*_Prev = __list;
            __list                 = __list->*_Next;
        }
        __result.__tail_ = __list;
        return __result;
    }

    _Item *__head_ = nullptr;
    _Item *__tail_ = nullptr;
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)
