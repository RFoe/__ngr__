#pragma once

// NOLINTBEGIN(google-runtime-int, cppcoreguidelines-pro-type-reinterpret-cast)
namespace __ngr::inline __v0::__core {

template <typename _Par, typename _Mem>
constexpr auto __compa_abi_offset(const _Mem _Par::*__mem_ptr) noexcept
    -> __PTRDIFF_TYPE__ {
#if defined(_MSC_VER)
    union {
        const _Mem _Par::*__mp_;
        int               __offset_;
    } __dummy;

    static_assert(sizeof __dummy == sizeof(int));

    __dummy.__mp_ = __mem_ptr;
    return static_cast<__PTRDIFF_TYPE__>(__dummy.__offset_);
#elif defined(__GNUC__) || defined(__HP_aCC) || defined(__IBMCPP__) ||         \
    defined(__DECCXX)
    // NOLINTBEGIN(bugprone-casting-through-void)
    const _Par *const __bp = nullptr;
    const char *const __mp = static_cast<const char *>(
        static_cast<const void *>(&(__bp->*__mem_ptr)));
    return static_cast<__PTRDIFF_TYPE__>(
        __mp - static_cast<const char *>(static_cast<const void *>(__bp)));
    // NOLINTEND(bugprone-casting-through-void)
#else
    union {
        const _Mem _Par::*__mp_;
        __PTRDIFF_TYPE__  __offset_;
    } __dummy;
    __dummy.__mp_ = __mem_ptr;
    return __dummy.__offset_ - 1;
#endif
}

template <typename _Par, typename _Mem>
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]] //
inline constexpr auto
__base_offset(_Mem *__mp, const _Mem _Par::*__mem_ptr) noexcept -> _Par * {
    return __builtin_launder(
        reinterpret_cast<_Par *>(
            reinterpret_cast<unsigned long long>(__mp) -
            static_cast<unsigned long long>(__compa_abi_offset(__mem_ptr))));
}

template <typename _Par, typename _Mem>
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]] //
inline constexpr auto
__base_offset(const _Mem *__mp, const _Mem _Par::*__mem_ptr) noexcept
    -> const _Par * {
    return __builtin_launder(
        reinterpret_cast<const _Par *>(
            reinterpret_cast<unsigned long long>(__mp) -
            static_cast<unsigned long long>(__compa_abi_offset(__mem_ptr))));
}

} // namespace __ngr::inline __v0::__core
// NOLINTEND(google-runtime-int, cppcoreguidelines-pro-type-reinterpret-cast)
