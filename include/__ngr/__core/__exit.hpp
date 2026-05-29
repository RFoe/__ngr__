#pragma once

#include <__concepts/invocable.h>
#include <__coroutine/coroutine_handle.h>
#include <__coroutine/coroutine_traits.h>
#include <__coroutine/trivial_awaitables.h>
#include <__utility/exchange.h>

#include <__ngr/__core/__coro_destroy.hpp>

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {
namespace __exit {
template <typename... _Ts> struct [[__nodiscard__]] __task {
    struct __promise;
    using promise_type = __promise;
    std::coroutine_handle<__promise> __coro_;

    __task(std::coroutine_handle<__promise> __h) noexcept : __coro_(__h) {}
    __task(__task &&__that) noexcept
        : __coro_(std::exchange(__that.__coro_, {})) {}
    ~__task() {
        if (__coro_) { __coro_destroy(__coro_); }
    }
    static constexpr auto await_ready() noexcept -> bool { return false; }
    template <typename _Pr>
    auto await_suspend(std::coroutine_handle<_Pr> __parent) noexcept -> bool {
        __coro_.promise().__continuation_  = __parent.promise().__continuation_;
        __parent.promise().__continuation_ = __coro_;
        return false;
    }
    auto await_resume() noexcept -> std::tuple<_Ts &...> {
        return std::exchange(__coro_, {}).promise().__args_;
    }

    struct __final_awaiter {
        static constexpr auto await_ready() noexcept -> bool { return false; }

        static auto await_suspend(std::coroutine_handle<__promise> __h) noexcept
            -> std::coroutine_handle<> {
            return __coro_destroy_and_resume(
                __h, __h.promise().__continuation_);
        }
        static constexpr void await_resume() noexcept {}
    };
    struct __promise {
        template <typename _Func> __promise(_Func &&, _Ts &...__ts) noexcept
            : __args_{__ts...} {}

        static auto initial_suspend() noexcept -> std::suspend_always {
            return {};
        }
        static auto final_suspend() noexcept -> __final_awaiter { return {}; }
        static void return_void() noexcept {}
        static void unhandled_exception() noexcept { __builtin_abort(); }
        auto        get_return_object() noexcept -> __task {
            return {std::coroutine_handle<__promise>::from_promise(*this)};
        }

        std::tuple<_Ts &...>    __args_{};
        std::coroutine_handle<> __continuation_ = nullptr;
    };
};
struct __impl {
    template <typename _Func, typename... _Ts>
    static auto _S_coro(_Func __func, _Ts... __args) noexcept
        -> __task<_Ts...> {
        (void)(__func)(__args...);
        co_return;
    }
    template <typename _Func, typename... _Ts>
        requires std::invocable<std::decay_t<_Func> &, std::decay_t<_Ts> &...>
    auto operator()(_Func &&__func, _Ts &&...__args) const noexcept
        -> __task<_Ts...> {
        return _S_coro(
            std::forward<_Func>(__func), std::forward<_Ts>(__args)...);
    }
};
}; // namespace __exit
inline constexpr __exit::__impl __exit_;
} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)