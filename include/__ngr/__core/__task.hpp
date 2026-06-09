#pragma once

#include <__coroutine/coroutine_traits.h>
#include <__coroutine/noop_coroutine_handle.h>
#include <__coroutine/trivial_awaitables.h>
#include <__exception/exception_ptr.h>
#include <__new/allocate.h>
#include <__type_traits/is_trivially_destructible.h>
#include <cassert>

#include <__ngr/__core/__aligned_storage.hpp>
#include <__ngr/__core/__allocator.hpp>
#include <__ngr/__core/__coro_destroy.hpp>
#include <__ngr/__core/__stop_inplace.hpp>
#include <__ngr/__core/__task_facade.hpp>

// NOLINTBEGIN(*-special-member-functions)
// NOLINTBEGIN(cert-dcl54-cpp, *-new-delete-overloads, *-new-delete-operators)
// NOLINTBEGIN(performance-unnecessary-value-param)
namespace __ngr::inline __v0::__core {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-new-delete"
struct __promise_base {
    struct __final_awaiter {
        static auto await_ready() noexcept -> bool { return false; }
        template <typename _Promise>
        auto await_suspend(std::coroutine_handle<_Promise> __cur) noexcept
            -> std::coroutine_handle<> {
            __cur.promise().__fwd_._M_destroy();
            return __cur.promise().__continuation_;
        }
        static void await_resume() noexcept {}
    };

    using __forward_stop_request = __stop_callback<__forward_stop_request<>>;
    std::coroutine_handle<>                   __continuation_ = std::noop_coroutine();
    __stop_source                             __source_{};
    __aligned_storage<__forward_stop_request> __fwd_;
    static auto initial_suspend() noexcept -> std::suspend_always { return {}; }
    static auto final_suspend() noexcept -> __final_awaiter { return {}; }

    template <typename _Promise>
    void _M_on_await_suspend(std::coroutine_handle<_Promise> __parent) noexcept {
        __continuation_ = __parent;
        __fwd_._M_construct(__parent.promise().__source_._M_get_stop_token(), __source_);
    }
};
template <typename _Ty = void> struct __promise_generic_storage {
    enum struct _Sg : unsigned char { _S_none, _S_error, _S_result };
    __extension__ union {
        std::exception_ptr __error_;
        _Ty                __result_;
    };
    _Sg __state_;
    __promise_generic_storage() noexcept : __state_(_Sg::_S_none) {}
    ~__promise_generic_storage() noexcept {
        switch (__state_) {
        case _Sg::_S_none: __builtin_unreachable();
        case _Sg::_S_error:
            [[__unlikely__]] //
            __error_.std::exception_ptr::~exception_ptr();
            return;
        case _Sg::_S_result:
            if constexpr (!std::is_trivially_destructible_v<_Ty>) { __result_.~_Ty(); }
            return;
        }
    }
    template <typename _Uy = _Ty> constexpr void return_value(_Uy &&__r) noexcept {
        ::new (&__result_) _Ty{std::forward<_Uy>(__r)};
        __state_ = _Sg::_S_result;
    }
    constexpr void _M_set_exception(std::exception_ptr __eptr) noexcept {
        ::new (&__error_) std::exception_ptr{__eptr};
        __state_ = _Sg::_S_error;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline constexpr auto _M_result() noexcept -> _Ty {
        return std::move(__result_);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline constexpr void _M_throw_uncaught() const {
        if (__state_ == _Sg::_S_error) [[__unlikely__]] {
            std::rethrow_exception(__error_);
        }
    }
};
template <> struct __promise_generic_storage<void> {
    std::exception_ptr __except_;
    constexpr void     return_void() noexcept {}
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    constexpr void _M_set_exception(std::exception_ptr __eptr) noexcept {
        __except_ = __eptr;
    }
    constexpr void _M_result() noexcept {}
    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline constexpr void _M_throw_uncaught() const {
        if (__except_) [[__unlikely__]] { std::rethrow_exception(__except_); }
    }
};

struct __promise_new_delete {
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static auto __alloc_address(void *__fr, std::size_t __n) noexcept
        -> __allocator ** {
        char *__r = static_cast<char *>(__fr) + __n;        // NOLINT
        return reinterpret_cast<__allocator **>(__round_up(__r, alignof(__allocator *)));
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static auto __allocate_size(std::size_t __n) noexcept -> std::size_t {
        return __round_up(__n, alignof(__allocator *)) + sizeof(__allocator *);
    }

    [[using __gnu__: __returns_nonnull__, __alloc_size__(1)]]
    auto operator new(const std::size_t __n) noexcept -> void * {
        const bool   __top_frame = (__k_tls_alloc == nullptr);
        __allocator *__pa __attribute__((__uninitialized__)); // NOLINT
        if (__top_frame) [[__unlikely__]] {
            __pa          = ::new (std::nothrow) __allocator{};
            __k_tls_alloc = __pa;
        } else {
            __pa = __k_tls_alloc;
        }

        void *__r                  = __pa->_M_push_stack_frame(__allocate_size(__n));
        *__alloc_address(__r, __n) = __top_frame ? __pa : nullptr;
        return __r;
    }

    [[__gnu__::__nonnull__]]
    void operator delete(void *__ptr, std::size_t __n) noexcept {
        __allocator *__alloc = *__alloc_address(__ptr, __n);
        if (__alloc != nullptr) [[__unlikely__]] {
            __alloc->_M_pop_stack_frame(__ptr);
            __k_tls_alloc = nullptr;
            __alloc->~__allocator();
            ::operator delete(__alloc, std::nothrow);
        } else {
            __k_tls_alloc->_M_pop_stack_frame(__ptr);
        }
    }
};

template <typename _Ty = void, typename _Alloc = __allocator> struct [[__nodiscard__]]
__task {
    struct __promise;
    struct __awaiter;
    using __value      = _Ty;
    using promise_type = __promise;

    std::coroutine_handle<__promise> __coro_{};

    __task(std::coroutine_handle<__promise> __h) noexcept : __coro_(__h) {}
    __task(__task &&__other) noexcept : __coro_(std::exchange(__other.__coro_, {})) {}
    auto operator=(__task &&__that) noexcept -> __task & = delete;

    ~__task() noexcept {
        if (__coro_) { __coro_destroy(__coro_); }
    }

    [[__nodiscard__]] auto operator co_await() && noexcept -> __awaiter {
        return __awaiter{std::move(*this)};
    }

    struct __awaiter final {
        __task __child_;
        __awaiter(__task __x) noexcept : __child_(std::move(__x)) {}

        static auto await_ready() noexcept -> bool { return false; }
        template <typename _Promise>
        auto await_suspend(std::coroutine_handle<_Promise> __parent) noexcept
            -> std::coroutine_handle<> {
            __child_.__coro_.promise()._M_on_await_suspend(__parent);
            return __child_.__coro_;
        }

        [[__nodiscard__]] auto await_resume() -> _Ty {
            auto &__promise = __child_.__coro_.promise();
            __promise._M_throw_uncaught();
            if constexpr (!std::is_void_v<_Ty>) { return __promise._M_result(); }
        }
    };

    struct __promise final
        : public __promise_generic_storage<_Ty>
        , public __promise_new_delete
        , public __promise_base {
        void unhandled_exception() noexcept {
            __promise_generic_storage<_Ty>::_M_set_exception(std::current_exception());
        }
        auto get_return_object() noexcept -> __task {
            return {std::coroutine_handle<__promise>::from_promise(*this)};
        }
    };
};
#pragma clang diagnostic pop
} // namespace __ngr::inline __v0::__core
// NOLINTEND(performance-unnecessary-value-param)
// NOLINTEND(cert-dcl54-cpp, *-new-delete-overloads, *-new-delete-operators)
// NOLINTEND(*-special-member-functions)
