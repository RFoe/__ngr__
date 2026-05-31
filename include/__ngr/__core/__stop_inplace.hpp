#pragma once

#include <__atomic/atomic.h>
#include <__thread/this_thread.h>
#include <__utility/exchange.h>
#include <cassert>
#include <cstdint>

// NOLINTBEGIN(*-special-member-functions, hicpp-no-assembler)
namespace __ngr::inline __v0::__core {

struct __stop_source;
struct __stop_token;
template <typename _Fn> struct __stop_callback;

struct __pause_or_yield {
    static constexpr std::uint32_t __yield_threshold_ = 20;
    std::uint32_t                  __count_           = 0;

    constexpr __pause_or_yield() noexcept = default;
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static void _S_spin_loop_pause() noexcept {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        ::_mm_pause();
#elif defined(__i386__) || defined(__x86_64__) || defined(_M_X64)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield");
#elif defined(__powerpc64__)
        __asm__ __volatile__("or 27,27,27");
#endif
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline void _M_try() noexcept {
        if (__count_++ < __yield_threshold_) {
            _S_spin_loop_pause();
        } else {
            if (__count_ == 0) { // wrapped around
                __count_ = __yield_threshold_;
            }
            std::this_thread::yield();
        }
    }
};

struct __stop_cb_base {
    constexpr void _M_execute() noexcept { (*this->__execute_)(this); }

    using __execute_fn = void (*)(__stop_cb_base *) noexcept;

    constexpr explicit __stop_cb_base(
        __stop_source const *__src, __execute_fn __fn) noexcept
        : __source_(__src), __execute_(__fn) {}

    constexpr void _M_register_callback() noexcept;

    __stop_source const *__source_;
    __execute_fn         __execute_;
    __stop_cb_base      *__next_               = nullptr;
    __stop_cb_base     **__prev_               = nullptr;
    std::atomic<bool>    __callback_completed_ = false;
};

struct __stop_source {
    __stop_source() noexcept = default;
    ~__stop_source() noexcept;
    __stop_source(__stop_source &&) = delete;

    constexpr auto _M_get_stop_token() const noexcept -> __stop_token;

    auto _M_request_stop() noexcept -> bool;

    [[__nodiscard__]]
    auto _M_stop_requested() const noexcept -> bool {
        return (__state_.load(std::memory_order_acquire) &
                __stop_requested_flag_) != 0;
    }

    auto _M_lock() const noexcept -> std::uint8_t;
    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline void _M_unlock(std::uint8_t __prev) const noexcept;
    [[__nodiscard__]] auto
    _M_try_lock_unless_stop_requested(bool __set_stop_requested) const noexcept
        -> bool;
    [[__nodiscard__]] auto
         _M_try_push_front_callback(__stop_cb_base *) const noexcept -> bool;
    void _M_remove_callback(__stop_cb_base *) const noexcept;

    static constexpr std::uint8_t __none_flag_           = 0;
    static constexpr std::uint8_t __stop_requested_flag_ = 1;
    static constexpr std::uint8_t __locked_flag_         = 2;

    mutable std::atomic<std::uint8_t> __state_    = __none_flag_;
    mutable __stop_cb_base           *__callback_ = nullptr;
    bool                              __notify_   = false;
};

struct __stop_token {
    constexpr __stop_token() noexcept : __source_(nullptr) {}
    constexpr __stop_token(__stop_token const &) noexcept = default;
    constexpr __stop_token(__stop_token &&__other) noexcept
        : __source_(std::exchange(__other.__source_, {})) {}

    constexpr auto operator=(__stop_token const &) noexcept
        -> __stop_token & = default;

    constexpr auto operator=(__stop_token &&__other) noexcept
        -> __stop_token & {
        __source_ = std::exchange(__other.__source_, nullptr);
        return *this;
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline constexpr auto _M_stop_requested() const noexcept -> bool {
        return __source_ != nullptr && __source_->_M_stop_requested();
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline constexpr auto _M_stop_possible() const noexcept -> bool {
        return __source_ != nullptr;
    }

    constexpr void swap(__stop_token &__other) noexcept {
        std::swap(__source_, __other.__source_);
    }

    constexpr auto operator==(__stop_token const &) const noexcept
        -> bool = default;

    constexpr explicit __stop_token(__stop_source const *__src) noexcept
        : __source_(__src) {}

    __stop_source const *__source_;
};

inline constexpr auto __stop_source::_M_get_stop_token() const noexcept
    -> __stop_token {
    return __stop_token{this};
}
template <typename _Fn> struct __stop_callback : public __stop_cb_base {
    template <typename _Fn2>
        requires std::is_constructible_v<_Fn, _Fn2>
    explicit __stop_callback(__stop_token __tok, _Fn2 &&__fn)
        noexcept(std::is_nothrow_constructible_v<_Fn, _Fn2>)
        : __stop_cb_base(__tok.__source_, &__stop_callback::_S_execute_impl),
          __func_(std::forward<_Fn2>(__fn)) {
        _M_register_callback();
    }

    ~__stop_callback() noexcept {
        if (__source_ != nullptr) [[__likely__]] {
            __source_->_M_remove_callback(this);
        }
    }

    __stop_callback(__stop_callback &&)                     = delete;
    auto operator=(__stop_callback &&) -> __stop_callback & = delete;

    static void _S_execute_impl(__stop_cb_base *__cb) noexcept {
        std::move(static_cast<__stop_callback *>(__cb)->__func_)();
    }

    [[__no_unique_address__]] _Fn __func_;
};

template <typename _Fn> __stop_callback(__stop_token, _Fn)
    -> __stop_callback<_Fn>;

inline __stop_source::~__stop_source() noexcept {
    assert((__state_.load(std::memory_order_relaxed) & __locked_flag_) == 0);
    assert(__callback_ == nullptr);
}

inline auto __stop_source::_M_request_stop() noexcept -> bool {
    if (!_M_try_lock_unless_stop_requested(true)) { return true; }
    __notify_ = true;
    while (__callback_ != nullptr) {
        __stop_cb_base *__tmp = __callback_;
        __tmp->__prev_        = nullptr;
        __callback_           = __tmp->__next_;
        if (__callback_ != nullptr) { __callback_->__prev_ = &__callback_; }

        __state_.store(__stop_requested_flag_, std::memory_order_release);
        __tmp->_M_execute();
        __tmp->__callback_completed_.store(true, std::memory_order_release);
        _M_lock();
    }

    __state_.store(__stop_requested_flag_, std::memory_order_release);
    return false;
}

inline auto __stop_source::_M_lock() const noexcept -> std::uint8_t {
    __pause_or_yield __loop;
    std::uint8_t     __prev = __state_.load(std::memory_order_relaxed);
    do {
        while ((__prev & __locked_flag_) != 0) {
            __loop._M_try();
            __prev = __state_.load(std::memory_order_relaxed);
        }
    } while (!__state_.compare_exchange_weak(
        __prev, __prev | __locked_flag_, std::memory_order_acquire,
        std::memory_order_relaxed));
    return __prev;
}

inline void __stop_source::_M_unlock(std::uint8_t __prev) const noexcept {
    __state_.store(__prev, std::memory_order_release);
}

inline auto __stop_source::_M_try_lock_unless_stop_requested(
    bool __set_stop_requested) const noexcept -> bool {
    __pause_or_yield   __loop;
    std::uint8_t       __prev = __state_.load(std::memory_order_relaxed);
    const std::uint8_t __next = __set_stop_requested
                                  ? (__locked_flag_ | __stop_requested_flag_)
                                  : __locked_flag_;
    do {
        while (true) {
            if ((__prev & __stop_requested_flag_) != 0) { return false; }
            if (__prev == __none_flag_) [[__unlikely__]] { break; }
            __loop._M_try();
            __prev = __state_.load(std::memory_order_relaxed);
        }
    } while (!__state_.compare_exchange_weak(
        __prev, __next, std::memory_order_acq_rel, std::memory_order_relaxed));

    return true;
}

inline auto
__stop_source::_M_try_push_front_callback(__stop_cb_base *__cb) const noexcept
    -> bool {
    if (!_M_try_lock_unless_stop_requested(false)) { return false; }

    __cb->__next_ = __callback_;
    __cb->__prev_ = &__callback_;
    if (__callback_ != nullptr) { __callback_->__prev_ = &__cb->__next_; }
    __callback_ = __cb;

    _M_unlock(__none_flag_);
    return true;
}

inline void
__stop_source::_M_remove_callback(__stop_cb_base *__cb) const noexcept {
    const std::uint8_t __prev = _M_lock();

    if (__cb->__prev_ != nullptr) [[__likely__]] {
        *__cb->__prev_ = __cb->__next_;
        if (__cb->__next_ != nullptr) {
            __cb->__next_->__prev_ = __cb->__prev_;
        }
        _M_unlock(__prev);
    } else {
        const bool __notify = this->__notify_;
        _M_unlock(__prev);
        if (__notify) [[__unlikely__]] {
            __pause_or_yield __loop;
            while (
                !__cb->__callback_completed_.load(std::memory_order_acquire)) {
                __loop._M_try();
            }
        }
    }
}

inline constexpr void __stop_cb_base::_M_register_callback() noexcept {
    if (__source_ != nullptr) {
        if (!__source_->_M_try_push_front_callback(this)) {
            __source_ = nullptr;
            _M_execute();
        }
    }
}

template <typename _Src = __stop_source> struct __forward_stop_request {
    _Src &__stop_source_;
    void  operator()() const noexcept { __stop_source_._M_request_stop(); }
};

template <typename _Src> __forward_stop_request(_Src &)
    -> __forward_stop_request<_Src>;

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions, hicpp-no-assembler)
