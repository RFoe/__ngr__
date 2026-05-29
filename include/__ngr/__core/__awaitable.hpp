#pragma once

#include <__coroutine/coroutine_handle.h>
#include <__coroutine/coroutine_traits.h>
#include <__coroutine/noop_coroutine_handle.h>
#include <__coroutine/trivial_awaitables.h>
#include <__cstddef/byte.h>
#include <__exception/exception_ptr.h>
#include <__memory/allocator.h>
#include <__memory/allocator_arg_t.h>
#include <__memory/destroy.h>
#include <__stop_token/stop_callback.h>
#include <__stop_token/stop_source.h>
#include <__type_traits/is_trivially_destructible.h>
#include <__utility/exchange.h>
#include <cassert>

#include <__ngr/__core/__aligned_storage.hpp>
#include <__ngr/__core/__coro_destroy.hpp>
#include <__ngr/__core/__stop_inplace.hpp>

// NOLINTBEGIN(*-special-member-functions)
// NOLINTBEGIN(cert-dcl54-cpp, *-new-delete-overloads, *-new-delete-operators)
namespace __ngr::inline __v0::__core {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-new-delete"

template <typename _Ty> struct __promise_generic_value {
    enum class _Has : unsigned char { _S_none, _S_except, _S_result };
    __extension__ union {
        std::exception_ptr __except_;
        _Ty                __result_;
    };
    _Has __state_;
    __promise_generic_value() noexcept : __state_(_Has::_S_none) {}
    ~__promise_generic_value() noexcept {
        switch (__state_) {
        case _Has::_S_none: return;
        case _Has::_S_except:
            [[__unlikely__]] __except_.std::exception_ptr::~exception_ptr();
            return;
        case _Has::_S_result:
            [[__likely__]]
            if constexpr (!std::is_trivially_destructible_v<_Ty>) {
                __result_.~_Ty();
            }
            return;
        }
    }
    template <typename _Uy = _Ty> constexpr void return_value(_Uy &&__r)
        noexcept(std::is_nothrow_constructible_v<_Ty, _Uy &&>) {
        static_assert(std::is_constructible_v<_Ty, _Uy &&>);
        ::new (&__result_) _Ty{std::forward<_Uy>(__r)};
        assert(__state_ == _Has::_S_none);
        __state_ = _Has::_S_result;
    }
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    constexpr void _M_catch(std::exception_ptr __eptr) noexcept {
        ::new (&__except_) std::exception_ptr{__eptr};
        assert(__state_ == _Has::_S_none);
        __state_ = _Has::_S_except;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline constexpr auto _M_result()
        noexcept(std::is_nothrow_move_constructible_v<_Ty>) -> _Ty {
        assert(__state_ == _Has::_S_result);
        return std::move(__result_);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline constexpr void _M_throw_uncaught() const {
        if (__state_ == _Has::_S_except) [[__unlikely__]] {
            std::rethrow_exception(__except_);
        }
    }
};
template <> struct __promise_generic_value<void> {
    std::exception_ptr __except_;
    constexpr void     return_void() noexcept {}
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    constexpr void _M_catch(std::exception_ptr __eptr) noexcept {
        __except_ = __eptr;
    }
    constexpr void _M_result() noexcept {}
    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline constexpr void _M_throw_uncaught() const {
        if (__except_) [[__unlikely__]] { std::rethrow_exception(__except_); }
    }
};
struct __memblock {
    alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) std::byte
        __storage_[__STDCPP_DEFAULT_NEW_ALIGNMENT__];

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline static auto _M_n_block(std::size_t __sz) noexcept -> std::size_t {
        static constexpr auto __blksz = sizeof(__memblock);
        return (__sz + __blksz - 1) / __blksz;
    }
};

template <typename _Alloc>
concept __stateless_alloc =
    std::default_initializable<_Alloc> &&
    std::allocator_traits<_Alloc>::is_always_equal::value;

template <typename _Alloc> struct __promise_generic_alloc {
    using _Alloc_blk =
        std::allocator_traits<_Alloc>::template rebind_alloc<__memblock>;
    using _Alloc_blk_traits = std::allocator_traits<_Alloc_blk>;
    static_assert(
        std::is_pointer_v<typename _Alloc_blk_traits::pointer>,
        "Must use allocators for true pointers with awaitables");

    static auto
    _M_alloc_address(std::uintptr_t __fn, std::uintptr_t __fsz) noexcept
        -> _Alloc_blk * {
        const std::uintptr_t         __an = __fn + __fsz;
        static constexpr std::size_t __ba = alignof(_Alloc_blk);
        return reinterpret_cast<_Alloc_blk *>(
            ((__an + __ba - 1) / __ba) * __ba);
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline static auto _M_alloc_size(std::size_t __csz) noexcept
        -> std::size_t {
        static constexpr auto __ba = alignof(_Alloc_blk);
        return __csz + __ba + sizeof(_Alloc_blk);
    }

    static auto _M_allocate(_Alloc_blk __b, std::size_t __csz) -> void * {
        if constexpr (__stateless_alloc<_Alloc_blk>)
            return __b.allocate(__memblock::_M_n_block(__csz));
        else {
            const std::size_t __nsz =
                __memblock::_M_n_block(_M_alloc_size(__csz));
            auto       *__f  = __b.allocate(__nsz);
            const auto  __fn = reinterpret_cast<std::uintptr_t>(__f);
            _Alloc_blk *__an = _M_alloc_address(__fn, __csz);
            ::new (__an) _Alloc_blk(std::move(__b));
            return __f;
        }
    }

    auto operator new(std::size_t __sz) -> void *
        requires std::default_initializable<_Alloc_blk>
    {
        return _M_allocate(_Alloc_blk{}, __sz);
    }

    template <typename _Alloc2, typename... _Args> auto operator new(
        const std::size_t __sz, std::allocator_arg_t, const _Alloc2 &__a,
        const _Args &...) -> void * {
        static_assert(
            std::convertible_to<const _Alloc2 &, _Alloc>,
            "the allocator argument to the coroutine must be "
            "convertible to the awaitable's allocator type");
        return _M_allocate(_Alloc_blk(_Alloc(__a)), __sz);
    }

    template <typename _This, typename _Alloc2, typename... _Args>
    auto operator new(
        const std::size_t __sz, const _This &, std::allocator_arg_t,
        const _Alloc2    &__a, const _Args &...) -> void    *{
        static_assert(
            std::convertible_to<const _Alloc2 &, _Alloc>,
            "the allocator argument to the coroutine must be "
            "convertible to the awaitable's allocator type");
        return _M_allocate(_Alloc_blk(_Alloc(__a)), __sz);
    }

    void operator delete(void *__ptr, const std::size_t __csz) noexcept {
        if constexpr (__stateless_alloc<_Alloc_blk>) {
            _Alloc_blk __b;
            return __b.deallocate(
                reinterpret_cast<__memblock *>(__ptr),
                __memblock::_M_n_block(__csz));
        } else {
            const std::size_t __nsz =
                __memblock::_M_n_block(_M_alloc_size(__csz));
            const auto  __fn = reinterpret_cast<std::uintptr_t>(__ptr);
            _Alloc_blk *__an = _M_alloc_address(__fn, __csz);
            _Alloc_blk  __b(std::move(*__an));
            __an->~_Alloc_blk();
            __b.deallocate(reinterpret_cast<__memblock *>(__ptr), __nsz);
        }
    }
};

template <> class __promise_generic_alloc<void> {
    using __deallocate_fn = void (*)(void *, std::size_t);

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline static auto _M_dealloc_address(
        const std::uintptr_t __fn, const std::uintptr_t __fsz) noexcept
        -> __deallocate_fn * {
        const std::uintptr_t         __an = __fn + __fsz;
        static constexpr std::size_t __ba = alignof(__deallocate_fn);
        const std::uintptr_t __aligned    = ((__an + __ba - 1) / __ba) * __ba;
        return reinterpret_cast<__deallocate_fn *>(__aligned); // NOLINT
    }

    template <typename _Alloc_blk>
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline static auto _M_alloc_address(
        const std::uintptr_t __fn, const std::uintptr_t __fsz) noexcept
        -> _Alloc_blk *
        requires(!__stateless_alloc<_Alloc_blk>)
    {
        static constexpr auto __ba  = alignof(_Alloc_blk);
        __deallocate_fn      *__da  = _M_dealloc_address(__fn, __fsz);
        auto                  __aan = reinterpret_cast<std::uintptr_t>(__da);
        __aan += sizeof(__deallocate_fn);
        const std::uintptr_t __aligned = ((__aan + __ba - 1) / __ba) * __ba;
        return reinterpret_cast<_Alloc_blk *>(__aligned);
    }

    template <typename _Alloc_blk = void>
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline static auto _M_alloc_size(const std::size_t __csz) noexcept
        -> std::size_t {
        if constexpr (!std::same_as<_Alloc_blk, void>) {
            static constexpr std::size_t __aa = alignof(_Alloc_blk);
            static constexpr std::size_t __as = sizeof(_Alloc_blk);
            static constexpr std::size_t __ba = __aa + alignof(__deallocate_fn);
            return __csz + __ba + __as + sizeof(__deallocate_fn);
        } else {
            static constexpr auto __ba = alignof(__deallocate_fn);
            return __csz + __ba + sizeof(__deallocate_fn);
        }
    }

    template <typename _Alloc_blk> static void
    _S_deallocate_impl(void *__ptr, const std::size_t __csz) noexcept {
        const std::size_t __asz  = _M_alloc_size<_Alloc_blk>(__csz);
        const std::size_t __nblk = __memblock::_M_n_block(__asz);

        if constexpr (__stateless_alloc<_Alloc_blk>) {
            _Alloc_blk __b;
            __b.deallocate(reinterpret_cast<__memblock *>(__ptr), __nblk);
        } else {
            const auto  __fn = reinterpret_cast<std::uintptr_t>(__ptr);
            _Alloc_blk *__an = _M_alloc_address<_Alloc_blk>(__fn, __csz);
            _Alloc_blk  __b(std::move(*__an));
            __an->~_Alloc_blk();
            __b.deallocate(reinterpret_cast<__memblock *>(__ptr), __nblk);
        }
    }

    template <typename _Alloc>
    static auto _M_allocate(const _Alloc &__a, const std::size_t __csz)
        -> void * {
        using _Alloc_blk =
            std::allocator_traits<_Alloc>::template rebind_alloc<__memblock>;
        using _Alloc_blk_traits = std::allocator_traits<_Alloc_blk>;

        static_assert(
            std::is_pointer_v<typename _Alloc_blk_traits::pointer>,
            "Must use allocators for true pointers with awaitables");

        __deallocate_fn   __d    = &_S_deallocate_impl<_Alloc_blk>;
        const auto        __b    = static_cast<_Alloc_blk>(__a);
        const std::size_t __asz  = _M_alloc_size<_Alloc_blk>(__csz);
        const std::size_t __nblk = __memblock::_M_n_block(__asz);
        void             *__p    = __b.allocate(__nblk);
        const auto        __pn   = reinterpret_cast<std::uintptr_t>(__p);
        *_M_dealloc_address(__pn, __csz) = __d;
        if constexpr (!__stateless_alloc<_Alloc_blk>) {
            _Alloc_blk *__an = _M_alloc_address<_Alloc_blk>(__pn, __csz);
            ::new (__an) _Alloc_blk(std::move(__b));
        }
        return __p;
    }

    auto operator new(const std::size_t __sz) -> void * {
        const std::size_t __nsz = _M_alloc_size<>(__sz);
        __deallocate_fn   __d   = +[](void *__ptr, std::size_t __sz) -> void {
            ::operator delete(__ptr, _M_alloc_size<>(__sz));
        };
        void      *__p                  = ::operator new(__nsz);
        const auto __pn                 = reinterpret_cast<uintptr_t>(__p);
        *_M_dealloc_address(__pn, __sz) = __d;
        return __p;
    }

    template <typename _Alloc, typename... _Args> auto operator new(
        const std::size_t __sz, std::allocator_arg_t, const _Alloc &__a,
        const _Args &...) -> void * {
        return _M_allocate(__a, __sz);
    }

    template <typename _This, typename _Alloc, typename... _Args>
    auto operator new(
        const std::size_t __sz, const _This &, std::allocator_arg_t,
        const _Alloc     &__a, const _Args &...) -> void     *{
        return _M_allocate(__a, __sz);
    }

    void operator delete(void *__ptr, const std::size_t __sz) noexcept {
        __deallocate_fn __dealloc __attribute__((__uninitialized__)); // NOLINT
        const auto      __pn = reinterpret_cast<uintptr_t>(__ptr);
        __dealloc            = *_M_dealloc_address(__pn, __sz);
        (*__dealloc)(__ptr, __sz);
    }
};

template <typename _Ty = void, typename _Alloc = std::allocator<std::byte>>
struct [[__nodiscard__]] __awaitable {
    struct __promise;
    struct __awaiter;

    using promise_type [[__gnu__::__nodebug__]]   = __promise;
    using value_type [[__gnu__::__nodebug__]]     = _Ty;
    using allocator_type [[__gnu__::__nodebug__]] = _Alloc;

    std::coroutine_handle<__promise> __coro_{};

    __awaitable(std::coroutine_handle<__promise> __h) noexcept : __coro_(__h) {}
    __awaitable(__awaitable &&__that) noexcept
        : __coro_(std::exchange(__that.__coro_, {})) {}

    auto operator=(__awaitable &&__that) noexcept -> __awaitable & = delete;

    ~__awaitable() noexcept {
        if (__coro_) { __coro_destroy(__coro_); }
    }

    auto _M_request_stop() noexcept -> bool {
        return __coro_ && __coro_.promise().__source_._M_request_stop();
    }
    [[__nodiscard__]] auto operator co_await() && noexcept -> __awaiter {
        return __awaiter{std::move(*this)};
    }

    struct __awaiter {
        __awaitable __this_;

        explicit __awaiter(__awaitable __x) noexcept
            : __this_(std::move(__x)) {}

        static auto await_ready() noexcept -> bool { return false; }
        template <typename _Previous>
        auto await_suspend(std::coroutine_handle<_Previous> __prev) noexcept
            -> std::coroutine_handle<> {
            __promise &__current      = __this_.__coro_.promise();
            __current.__continuation_ = __prev;
            __current.__fwd_._M_construct(
                __prev.promise().__source_._M_get_stop_token(),
                __current.__source_);
            return __this_.__coro_;
        }

        [[__nodiscard__]] auto await_resume() -> _Ty {
            __promise &__current = __this_.__coro_.promise();
            __current._M_throw_uncaught();
            if constexpr (!std::is_void_v<_Ty>) {
                return __current._M_result();
            }
        }
    };

    struct __promise final
        : public __promise_generic_value<_Ty>
        , public __promise_generic_alloc<_Alloc> {
        struct __final_awaiter {
            [[__nodiscard__]]
            auto await_ready() const noexcept -> bool {
                return false;
            }
            auto await_suspend(std::coroutine_handle<__promise> __curr) noexcept
                -> std::coroutine_handle<> {
                __curr.promise().__fwd_._M_destroy();
                return __curr.promise().__continuation_;
            }
            void await_resume() const noexcept {}
        };
        using __forward_stop_request [[__gnu__::__nodebug__]] =
            __stop_callback<__forward_stop_request<>>;
        std::coroutine_handle<> __continuation_ = std::noop_coroutine();
        __stop_source           __source_{};
        __aligned_storage<__forward_stop_request> __fwd_;

        auto get_return_object() noexcept -> __awaitable {
            return {std::coroutine_handle<__promise>::from_promise(*this)};
        }
        static auto initial_suspend() noexcept -> std::suspend_always {
            return {};
        }
        static auto final_suspend() noexcept -> __final_awaiter { return {}; }
        void        unhandled_exception() noexcept {
            __promise_generic_value<_Ty>::_M_catch(std::current_exception());
        }
        template <typename _Awaitable>
        auto await_transform(_Awaitable &&__t) noexcept -> _Awaitable && {
            return static_cast<_Awaitable &&>(__t);
        }
    };
};

#pragma clang diagnostic pop
} // namespace __ngr::inline __v0::__core
// NOLINTEND(cert-dcl54-cpp, *-new-delete-overloads, *-new-delete-operators)
// NOLINTEND(*-special-member-functions)
