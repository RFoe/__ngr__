#pragma once

#include <__atomic/atomic.h>
#include <__cstddef/ptrdiff_t.h>
#include <cassert>
#include <cstdint>

/// @file __intrusive_rcu_list.hpp
/// @brief Lock-free intrusive singly-linked list.
///
/// Concurrency contract:
///   - Multiple writers may call _M_push_front / _M_erase concurrently.
///   - A single reader may call _M_loop_ro_forward concurrently with
///     any number of writers.
///   - Readers are wait-free.  Writers are lock-free (CAS loops).
///   - Nodes removed via _M_erase must NOT be freed until all concurrent
///     readers have finished (grace period).  The caller is responsible
///     for the reclamation strategy (EBR, hazard pointers, QSBR, …).
///
/// Design (Harris-style marked pointers):
///   - Each node's `_Next` pointer is accessed atomically via reinterpret
///     cast (safe per C++20 [atomics.types.generic]/3).
///   - The lowest bit of the `next` pointer serves as a logical-deletion
///     mark.  A marked node is logically removed and will be physically
///     unlinked by subsequent push / erase / loop operations.
///   - This eliminates the need for a `pprev` back-link, making all
///     operations safe under multiple concurrent writers.

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {

template <auto _Next> struct __intrusive_rcu_list;

template <typename _Tp, _Tp *_Tp::*_Next> struct __intrusive_rcu_list<_Next> {
    using __pointer        = _Tp *;
    using __atomic_pointer = std::atomic<_Tp *>;
    using __uintptr        = std::uintptr_t;

    // ==================================================================
    //  Marked-pointer helpers
    // ==================================================================
    //  Bit 0 of a node pointer is the "logically deleted" mark.
    //  _Tp* is at least 2-byte aligned (usually 8), so bit 0 is free.
    // ==================================================================

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static auto _S_is_marked(__pointer __p) noexcept -> bool {
        return (reinterpret_cast<__uintptr>(__p) & 1U) != 0; // NOLINT
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static auto _S_marked(__pointer __p) noexcept -> __pointer {
        return reinterpret_cast<__pointer>(                   // NOLINT
            reinterpret_cast<__uintptr>(__p) | 1U);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static auto _S_unmarked(__pointer __p) noexcept -> __pointer {
        return reinterpret_cast<__pointer>(                   // NOLINT
            reinterpret_cast<__uintptr>(__p) & ~__uintptr{1});
    }

    // ==================================================================
    //  Atomic access to the _Next member
    // ==================================================================

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static auto
    _S_load_next(__pointer __node, std::memory_order __mo) noexcept
        -> __pointer {
        auto *__ap = reinterpret_cast<__atomic_pointer *>( // NOLINT
            &(__node->*_Next));
        return __ap->load(__mo);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static void _S_store_next(
        __pointer __node, __pointer __val, std::memory_order __mo) noexcept {
        auto *__ap = reinterpret_cast<__atomic_pointer *>( // NOLINT
            &(__node->*_Next));
        __ap->store(__val, __mo);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static auto _S_cas_next(
        __pointer __node, __pointer &__expected, __pointer __desired) noexcept
        -> bool {
        auto *__ap = reinterpret_cast<__atomic_pointer *>( // NOLINT
            &(__node->*_Next));
        return __ap->compare_exchange_strong(
            __expected, __desired, std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    // ==================================================================
    //  Head pointer
    // ==================================================================
    __atomic_pointer __head_{nullptr};

    // ==================================================================
    //  _S_find  —  internal search helper
    // ==================================================================
    /// Walk the list from `__head_` looking for `__target`.
    /// Along the way, physically unlink any logically-deleted nodes
    /// (opportunistic cleanup — keeps the list short).
    ///
    /// Returns:
    ///   __prev_loc  — pointer to the atomic slot (head or some node's
    ///                 next) that currently points to `__target`.
    ///   __cur       — `__target` if found, nullptr otherwise.
    ///
    /// This is the Harris-list "find" operation adapted for a singly-
    /// linked intrusive list.
    struct __find_result {
        __atomic_pointer *__prev_loc;
        __pointer         __cur;
    };

    auto _M_find(__pointer __target) noexcept -> __find_result {
    __retry:
        __atomic_pointer *__prev = &__head_;
        __pointer         __cur  = __prev->load(std::memory_order_acquire);

        while (__cur != nullptr) {
            __pointer __next = _S_load_next(__cur, std::memory_order_acquire);

            if (_S_is_marked(__next)) {
                // __cur is logically deleted — try to unlink it physically.
                __pointer __clean_next = _S_unmarked(__next);
                __pointer __expected   = __cur;
                if (!__prev->compare_exchange_strong(
                        __expected, __clean_next, std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    // Another writer changed *prev; restart.
                    goto __retry; // NOLINT
                }
                // __cur successfully unlinked; do NOT advance __prev.
                __cur = __clean_next;
                continue;
            }

            if (__cur == __target) { return {__prev, __cur}; }

            __prev = reinterpret_cast<__atomic_pointer *>( // NOLINT
                &(__cur->*_Next));
            __cur  = __next;
        }

        return {__prev, nullptr};
    }

    // ==================================================================
    //  _M_push_front  —  lock-free head-insert (multiple writers OK)
    // ==================================================================
    /// Insert `__node` at the head of the list.
    ///
    /// Precondition: `__node` is not currently in any list.
    /// Lock-free: uses a CAS loop on `__head_`.
    void _M_push_front(__pointer __node) noexcept {
        assert(__node != nullptr);

        __pointer __old_head = __head_.load(std::memory_order_relaxed);
        do {
            _S_store_next(__node, __old_head, std::memory_order_relaxed);
        } while (!__head_.compare_exchange_weak(
            __old_head, __node, std::memory_order_release,
            std::memory_order_relaxed));
    }

    // ==================================================================
    //  _M_erase  —  lock-free logical + physical deletion
    // ==================================================================
    /// Remove `__node` from the list.
    ///
    /// Step 1 (logical delete):  CAS the node's `next` pointer to set
    ///         the mark bit.  After this, concurrent readers can still
    ///         traverse through the node (its `next` is still valid,
    ///         modulo the mark bit which readers strip).
    ///
    /// Step 2 (physical unlink): Use `_M_find` to walk from head and
    ///         splice the node out.  If this CAS fails (another writer
    ///         already unlinked it), that's fine.
    ///
    /// Returns true if this call performed the logical deletion,
    /// false if the node was already marked (deleted by another writer).
    ///
    /// The caller MUST defer freeing `__node` until the grace period
    /// has elapsed.
    auto _M_erase(__pointer __node) noexcept -> bool {
        assert(__node != nullptr);

        // Step 1: logical delete — mark the node's next pointer.
        __pointer __next = _S_load_next(__node, std::memory_order_acquire);
        while (true) {
            if (_S_is_marked(__next)) {
                // Already logically deleted by someone else.
                return false;
            }
            // CAS: next → next|MARK
            if (_S_cas_next(__node, __next, _S_marked(__next))) { break; }
            // __next has been updated by _S_cas_next (spurious or real
            // change); retry.
        }

        // Step 2: physical unlink — best-effort.
        // _M_find will walk from head and unlink all marked nodes it
        // encounters (including ours).
        _M_find(__node);

        return true;
    }

    // ==================================================================
    //  _M_loop_ro_forward  —  wait-free read-side traversal
    // ==================================================================
    /// Traverse the list in forward direction.  Safe to call concurrently
    /// with any number of writers calling _M_push_front / _M_erase.
    ///
    /// Logically-deleted (marked) nodes are skipped transparently.
    ///
    /// @tparam _Fn  Callable with signature `void(_Tp &)` or
    ///              `bool(_Tp &)`.  Returning `false` stops early.
    ///
    /// The reader never modifies any pointer — it is truly read-only and
    /// wait-free.
    template <typename _Fn> void _M_loop_ro_forward(_Fn &&__fn) const noexcept {
        __pointer __cur = __head_.load(std::memory_order_acquire);

        while (__cur != nullptr) {
            __pointer __raw_next =
                _S_load_next(__cur, std::memory_order_acquire);
            __pointer __next = _S_unmarked(__raw_next);

            // Skip logically-deleted nodes.
            if (!_S_is_marked(__raw_next)) {
                if constexpr (requires {
                                  { __fn(*__cur) } -> std::same_as<bool>;
                              }) {
                    if (!__fn(*__cur)) { return; }
                } else {
                    __fn(*__cur);
                }
            }

            __cur = __next;
        }
    }

    // ==================================================================
    //  Observers
    // ==================================================================

    [[__nodiscard__]]
    auto _M_empty() const noexcept -> bool {
        // A list is empty when there are no *unmarked* reachable nodes.
        // For a fast check, just test the head (may include marked nodes).
        __pointer __cur = __head_.load(std::memory_order_acquire);
        while (__cur != nullptr) {
            __pointer __next = _S_load_next(__cur, std::memory_order_acquire);
            if (!_S_is_marked(__next)) { return false; }
            __cur = _S_unmarked(__next);
        }
        return true;
    }

    // ==================================================================
    //  Read-side iterator
    // ==================================================================
    /// Forward iterator that performs acquire loads and skips marked
    /// (logically-deleted) nodes.  Suitable for single-reader traversal
    /// concurrent with multiple writers.
    struct __ro_iterator {
        using difference_type = std::ptrdiff_t;
        using value_type      = _Tp;
        using pointer         = _Tp *;
        using reference       = _Tp &;

        __pointer __cur_ = nullptr;

        constexpr __ro_iterator() noexcept = default;
        constexpr explicit __ro_iterator(__pointer __p) noexcept : __cur_(__p) {
            _M_skip_marked();
        }

        [[__nodiscard__]]
        auto operator*() const noexcept -> reference {
            assert(__cur_ != nullptr);
            return *__cur_;
        }

        [[__nodiscard__]]
        auto operator->() const noexcept -> pointer {
            assert(__cur_ != nullptr);
            return __cur_;
        }

        auto operator++() noexcept -> __ro_iterator & {
            assert(__cur_ != nullptr);
            __pointer __raw = _S_load_next(__cur_, std::memory_order_acquire);
            __cur_          = _S_unmarked(__raw);
            _M_skip_marked();
            return *this;
        }

        auto operator++(int) noexcept -> __ro_iterator {
            auto __tmp = *this;
            ++*this;
            return __tmp;
        }

        friend auto
        operator==(__ro_iterator const &__a, __ro_iterator const &__b) noexcept
            -> bool = default;

      private:
        /// Advance past any nodes whose next pointer is marked (i.e.
        /// the node itself is logically deleted).
        void _M_skip_marked() noexcept {
            while (__cur_ != nullptr) {
                __pointer __raw =
                    _S_load_next(__cur_, std::memory_order_acquire);
                if (!_S_is_marked(__raw)) { break; }
                __cur_ = _S_unmarked(__raw);
            }
        }
    };

    [[__nodiscard__]]
    auto _M_ro_begin() const noexcept -> __ro_iterator {
        return __ro_iterator{__head_.load(std::memory_order_acquire)};
    }

    [[__nodiscard__]]
    static constexpr auto _M_ro_end() noexcept -> __ro_iterator {
        return __ro_iterator{};
    }
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)
