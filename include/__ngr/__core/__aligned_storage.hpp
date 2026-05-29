#pragma once

#include <__memory/destroy.h>
#include <new>
#include <type_traits>

namespace __ngr::inline __v0::__core {
template <typename _Ty> struct __aligned_storage {
    template <typename... _Args> constexpr auto _M_construct(_Args &&...__args)
        noexcept(std::is_nothrow_constructible_v<_Ty, _Args...>) -> _Ty & {
        return *std::launder(::new (static_cast<void *>(__buffer_))
                                 _Ty{static_cast<_Args &&>(__args)...});
    }

    constexpr void _M_destroy() noexcept { std::destroy_at(&_M_get()); }

    constexpr auto _M_get() & noexcept -> _Ty & {
        return *reinterpret_cast<_Ty *>(__buffer_);
    }
    constexpr auto _M_get() && noexcept -> _Ty && {
        return static_cast<_Ty &&>(*reinterpret_cast<_Ty *>(__buffer_));
    }
    constexpr auto _M_get() const & noexcept -> _Ty const & {
        return *reinterpret_cast<_Ty const *>(__buffer_);
    }
    constexpr auto _M_get() const && noexcept -> _Ty const && = delete;

    constexpr auto operator->() noexcept -> _Ty * {
        return reinterpret_cast<_Ty *>(__buffer_);
    }

    constexpr auto operator->() const noexcept -> _Ty const * {
        return reinterpret_cast<_Ty const *>(__buffer_);
    }

    alignas(_Ty) unsigned char __buffer_[sizeof(_Ty)]{};
};

template <typename _Ref>
    requires std::is_reference_v<_Ref>
struct __aligned_storage<_Ref> {
    constexpr auto _M_construct(_Ref __ref) noexcept -> _Ref {
        __ptr_ = std::addressof(__ref);
        return static_cast<_Ref>(*__ptr_);
    }

    constexpr void _M_destroy() noexcept {}

    constexpr auto _M_get() const noexcept -> _Ref {
        return static_cast<_Ref>(*__ptr_);
    }

    constexpr auto operator->() const noexcept -> std::add_pointer_t<_Ref> {
        return __ptr_;
    }

    std::add_pointer_t<_Ref> __ptr_ = nullptr;
};

template <> struct __aligned_storage<void> {
    constexpr __aligned_storage() noexcept = default;

    template <typename... _Args>
    constexpr void _M_construct(_Args &&...) noexcept {}

    constexpr void _M_destroy() noexcept {}

    constexpr auto _M_get() const noexcept -> void {}

    constexpr auto operator->() const noexcept -> void * { return nullptr; }
};
} // namespace __ngr::inline __v0::__core
