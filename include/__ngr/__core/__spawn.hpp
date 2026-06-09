#pragma once

#include <__concepts/invocable.h>
#include <__coroutine/coroutine_traits.h>
#include <__coroutine/noop_coroutine_handle.h>
#include <__coroutine/trivial_awaitables.h>
#include <__utility/exchange.h>

#include <__ngr/__core/__coro_destroy.hpp>
#include <__ngr/__core/__task_facade.hpp>

// NOLINTBEGIN(*-special-member-functions)
namespace __ngr::inline __v0::__core {
template <typename _Ty, typename... _Ts> struct [[__nodiscard__]] __spawn_task {
    __task_facade *__task_ = nullptr;

    struct __promise;
    struct __final_awaiter {
        static constexpr auto await_ready() noexcept -> bool { return false; }
        static auto           await_suspend(std::coroutine_handle<__promise> __h) noexcept
            -> std::coroutine_handle<> {
            __coro_destroy(__h);
            return std::noop_coroutine();
        }
        static constexpr void await_resume() noexcept {}
    };
    struct __promise {
        __promise(_Ty &__r, [[__maybe_unused__]] _Ts &...__ts) noexcept {
            using __coro [[__gnu__::__nodebug__]] = std::coroutine_handle<__promise>;
            __task_.__resume_                     = __r.__coro_;
            __r.__coro_.promise().__continuation_ = {__coro::from_promise(*this)};
        }
        static auto   initial_suspend() noexcept -> std::suspend_always { return {}; }
        static auto   final_suspend() noexcept -> __final_awaiter { return {}; }
        static void   return_void() noexcept {}
        static void   unhandled_exception() noexcept { __builtin_abort(); }
        auto          get_return_object() noexcept -> __spawn_task { return {&__task_}; }
        __task_facade __task_;
    };
    using promise_type [[__gnu__::__nodebug__]] = __promise;
};
} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-special-member-functions)
