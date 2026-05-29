#include <__ngr/__core/__awaitable.hpp>
#include <__ngr/__core/__generic_task_base.hpp>
#include <__ngr/__core/__intrusive_queue.hpp>
#include <print>
#include <variant>
#include <vector>

using namespace __ngr::__core; // NOLINT

namespace {

auto __plus(const int __a, const int __b) noexcept -> __awaitable<int> {
    std::println("__plus(): co_return __a + __b");
    co_return __a + __b;
}

auto __println(const char *__msg) noexcept -> __awaitable<void> {
    std::println("__println(): {}", __msg);
    co_return;
}
__intrusive_queue<&__generic_task_base::__next_> __pending_;
struct __yield_awaiter : __generic_task_base {
    __yield_awaiter() noexcept {
        __execute_ = [](__generic_task_base *__base) noexcept -> void {
            static_cast<__yield_awaiter *>(__base)->__resume_.resume();
        };
    }
    std::coroutine_handle<> __resume_;
    static auto             await_ready() noexcept -> bool { return false; }
    auto await_suspend(std::coroutine_handle<void> __coro) noexcept
        -> std::coroutine_handle<> {
        __resume_ = __coro;
        __pending_._M_push_back(this);
        return std::noop_coroutine();
    }

    void await_resume() noexcept {}
};
struct __yield_until_stop_request_awaiter : __generic_task_base {
    __yield_until_stop_request_awaiter() noexcept {
        __execute_ = [](__generic_task_base *__base) noexcept -> void {
            static_cast<__yield_until_stop_request_awaiter *>(__base)
                ->__resume_.resume();
        };
    }
    std::coroutine_handle<> __resume_;
    struct __resume_submit_callback {
        __yield_until_stop_request_awaiter &__self_;
        void                                operator()() const noexcept {
            std::println(
                "__yield_until_stop_request_awaiter::stop_request seen, {}",
                __pending_._M_empty());
            __pending_._M_push_back(&__self_);
        }
    };
    using __suspend_resume_callback = __stop_callback<__resume_submit_callback>;
    __extension__ union {
        __suspend_resume_callback __callback_;
    };
    ~__yield_until_stop_request_awaiter() noexcept {}

    static auto await_ready() noexcept -> bool { return false; }
    template <typename _Self>
    auto await_suspend(std::coroutine_handle<_Self> __coro) noexcept
        -> std::coroutine_handle<> {
        __resume_ = __coro;
        ::new (&__callback_) __suspend_resume_callback(
            __coro.promise().__source_._M_get_stop_token(), *this);
        return std::noop_coroutine();
    }

    void await_resume() noexcept { __callback_.~__suspend_resume_callback(); }
};
auto __yield_until_stop_request() noexcept -> __awaitable<void> {
    std::println(
        "__yield_until_stop_request(): co_await "
        "__yield_until_stop_request_awaiter{{}};");
    co_await __yield_until_stop_request_awaiter{};
    std::println("__yield_until_stop_request(): co_return;");
    co_return;
};

auto __yield() noexcept -> __awaitable<void> {
    std::println(
        "__yield(): co_await "
        "__yield_awaiter{{}};");
    co_await __yield_awaiter{};
    std::println("__yield(): co_return;");
    co_return;
};
template <typename, typename... _Ts> struct __async_scope_awaiter;
template <typename... _Ts, std::size_t... _Indices>
struct __async_scope_awaiter<std::index_sequence<_Indices...>, _Ts...>
    : __generic_task_base {
    using __forward_stop_request = __stop_callback<__forward_stop_request<>>;
    template <std::size_t _Index> struct __resume_task : __generic_task_base {
        __async_scope_awaiter &__self_;
        __extension__ union {
            __forward_stop_request __fwd_;
        };
        __resume_task(__async_scope_awaiter &__self) noexcept
            : __self_(__self) {
            __execute_ = [](__generic_task_base *__base) noexcept -> void {
                std::get<_Index>(
                    static_cast<__resume_task *>(__base)->__self_.__awaitable_)
                    .__coro_.resume();
            };
        }
        ~__resume_task() noexcept {}
    };
    template <std::size_t _Index, typename _Ty> static auto
    __submit_facade_coro(_Ty __coro, __async_scope_awaiter *__awaiter) noexcept
        -> __awaitable<void> {
        const int __cnt = ++__awaiter->__count_;
        if (__cnt == 1) {
            if constexpr (std::is_void_v<typename _Ty::value_type>) {
                co_await std::move(__coro);
                __awaiter->__result_.template emplace<_Index>();
            } else {
                __awaiter->__result_.template emplace<_Index>(
                    co_await std::move(__coro));
            }
            std::println("co_await return");
            if (__cnt != sizeof...(_Ts)) {
                std::println("__awaiter->__source_._M_request_stop();");
                __awaiter->__source_._M_request_stop();
            }
        } else {
            (void)(co_await std::move(__coro));
        }
        if (__cnt == sizeof...(_Ts)) { __pending_._M_push_back(__awaiter); }
        co_return;
    }
    __async_scope_awaiter(_Ts... __args) noexcept
        : __result_(),
          __awaitable_(
              __submit_facade_coro<_Indices>(std::move(__args), this)...),
          __submit_(((void)__args, *this)...) {
        __execute_ = [](__generic_task_base *__base) noexcept -> void {
            static_cast<__async_scope_awaiter *>(__base)->__resume_.resume();
        };
    }
    ~__async_scope_awaiter() noexcept {
        std::println("~__async_scope_awaiter()");
    }
    template <typename _Ty = void> struct __object_wrapper_impl {
        using __t = _Ty;
    };
    template <> struct __object_wrapper_impl<void> {
        struct __empty {};
        using __t = __empty;
    };
    template <typename _Ty> using __void_task = __awaitable<void>;
    template <typename _Ty> using __object_wrapper =
        __object_wrapper_impl<_Ty>::__t;
    std::variant<__object_wrapper<typename _Ts::value_type>...> __result_;
    std::tuple<__void_task<_Ts>...>                             __awaitable_;
    std::tuple<__resume_task<_Indices>...>                      __submit_;
    std::coroutine_handle<> __resume_ = nullptr;
    __stop_source           __source_;
    int                     __count_ = 0;
    __extension__ union {
        __forward_stop_request __fwd_;
    };

    static auto await_ready() noexcept -> bool { return false; }
    template <typename _Self>
    auto await_suspend(std::coroutine_handle<_Self> __coro) noexcept
        -> std::coroutine_handle<> {
        __resume_ = __coro;
        auto __fn = [this]<std::size_t _Index>() noexcept -> void {
            ::new (&std::get<_Index>(__submit_).__fwd_) __forward_stop_request{
                __source_._M_get_stop_token(),
                std::get<_Index>(__awaitable_).__coro_.promise().__source_};
            __pending_._M_push_back(&std::get<_Index>(__submit_));
        };

        ::new (&__fwd_) __forward_stop_request(
            __coro.promise().__source_._M_get_stop_token(), __source_);
        (__fn.template operator()<_Indices>(), ...);
        return std::noop_coroutine();
    }

    auto await_resume() noexcept
        -> std::variant<__object_wrapper<typename _Ts::value_type>...> {
        auto __fn = [this]<std::size_t _Index>() noexcept -> void {
            std::get<_Index>(__submit_).__fwd_.~__forward_stop_request();
        };
        (__fn.template operator()<_Indices>(), ...);
        __fwd_.~__forward_stop_request();

        return std::move(__result_);
    }
};
template <typename... _Ts> auto __async_scope(_Ts... __args)
    -> __async_scope_awaiter<std::index_sequence_for<_Ts...>, _Ts...> {
    return {std::move(__args)...};
}

auto __async_main() noexcept -> __awaitable<void> {
    const int __result = co_await __plus(1, 2);
    assert(__result == 1 + 2);
    std::println("__async_main():     co_await __println(...);");
    co_await __println("hello, world");
    std::println(
        "__async_main(): co_await "
        "__async_scope(__println(...), "
        "__yield_until_stop_request, "
        "__yield_until_stop_request())");
    auto __var = co_await __async_scope(
        __yield(), __yield_until_stop_request(), __yield_until_stop_request());
    assert(__var.index() == 0);
    std::println("__async_main(): co_return");
    co_return;
}

} // namespace

auto main() -> int {
    auto __awaitable = __async_main();
    __awaitable.__coro_();
    (void)std::getchar();
    // __awaitable.__coro_();
    while (not __pending_._M_empty()) {
        auto __temp = std::move(__pending_);
        while (auto *__task = __temp._M_pop_front()) __task->_M_execute();
    }
    std::println("finish");
}
