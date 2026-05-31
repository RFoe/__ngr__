#pragma once

#include <linux/io_uring.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <__algorithm/clamp.h>
#include <__chrono/duration.h>
#include <__chrono/steady_clock.h>
#include <__system_error/system_error.h>

#include <__ngr/__core/__file_descriptor.hpp>
#include <__ngr/__core/__generic_coro_task.hpp>
#include <__ngr/__core/__intrusive_heap.hpp>
#include <__ngr/__core/__memory_mapped_region.hpp>

namespace __ngr::inline __v0::__core {

[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline void _S_throw_error_code_if(const bool __cond, const int __ec) {
    if (__cond) [[__unlikely__]] {
        throw std::system_error(__ec, std::system_category());
    }
}

inline auto _S_io_uring_setup(unsigned __entries, ::io_uring_params &__params)
    -> __file_descriptor {
    const int __rc =
        static_cast<int>(::syscall(__NR_io_uring_setup, __entries, &__params));
    _S_throw_error_code_if(__rc < 0, -__rc);
    return __file_descriptor{__rc};
}

inline auto _S_io_uring_enter(
    int __ring_fd, unsigned int __to_submit, unsigned int __min_complete,
    unsigned int __flags) -> int {
    const int __rc = static_cast<int>(::syscall(
        __NR_io_uring_enter, __ring_fd, __to_submit, __min_complete, __flags,
        nullptr, 0));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto _S_duration_to_timespec(std::chrono::nanoseconds __dur) noexcept
    -> __kernel_timespec {
    auto __sec = std::chrono::duration_cast<std::chrono::seconds>(__dur);
    __dur -= __sec;
    __sec = (std::max)(__sec, std::chrono::seconds{0});
    __dur = std::clamp(
        __dur, std::chrono::nanoseconds{0},
        std::chrono::nanoseconds{999'999'999});
    return __kernel_timespec{.tv_sec = __sec.count(), .tv_nsec = __dur.count()};
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto _S_io_uring_enter(
    int __ring_fd, unsigned int __to_submit, unsigned int __min_complete,
    unsigned int __flags, void *__arg, std::size_t __argsz) -> int {
    const int __rc = static_cast<int>(::syscall(
        __NR_io_uring_enter, __ring_fd, __to_submit, __min_complete, __flags,
        __arg, __argsz));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto _S_io_uring_register(
    int __ring_fd, unsigned __opcode, void *__arg, unsigned __nr) -> int {
    const int __rc = static_cast<int>(
        ::syscall(__NR_io_uring_register, __ring_fd, __opcode, __arg, __nr));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto _S_mmap(int __fd, ::off_t __offset, std::size_t __size)
    -> __memory_mapped_region {
    void *__ptr = ::mmap(
        nullptr, __size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
        __fd, __offset);
    _S_throw_error_code_if(__ptr == MAP_FAILED, errno);
    return __memory_mapped_region{__ptr, __size};
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto _S_umin(__u32 __a, __u32 __b) noexcept -> __u32 {
    return __a < __b ? __a : __b;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto _S_umax(__u32 __a, __u32 __b) noexcept -> __u32 {
    return __a < __b ? __b : __a;
}

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
template <typename _Ty>
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto _S_at_offset_as(void *__base, __u32 __offset) noexcept -> _Ty {
    return reinterpret_cast<_Ty>(static_cast<std::byte *>(__base) + __offset);
}
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

template <typename _Ty> struct __wrapper {
    consteval __wrapper(_Ty __v) noexcept : __v_(__v) {}
    _Ty __v_;
};
template <__wrapper _Vp> struct __nttp {};
template <__wrapper _Vp> inline constexpr __nttp<_Vp> _nttp_{};

struct __socket_task : __generic_coro_task {
    enum class _Sg : unsigned char { _S_submit, _S_fire };
    __extension__ union __params {
        ::io_uring_sqe       *__sqe_;
        const ::io_uring_cqe *__cqe_;
    };
    void (*__virt_)(__socket_task *, const _Sg, __params) noexcept;
    template <__wrapper __submit_, __wrapper __complete_> static void
    _S_delegate(__socket_task *__self, const _Sg __op, __params __p) noexcept {
        switch (__op) {
        case _Sg::_S_submit: (__submit_.__v_)(__self, *__p.__sqe_); return;
        case _Sg::_S_fire:   (__complete_.__v_)(__self, *__p.__cqe_); return;
        }
    }
    template <__wrapper __submit_, __wrapper __complete_>
    __socket_task(__nttp<__submit_>, __nttp<__complete_>) noexcept
        : __virt_(&_S_delegate<__submit_, __complete_>) {}
};
struct __generic_timer_task : __generic_coro_task {
    enum class _Sg : unsigned char { _S_submit, _S_stop };
    _Sg __cmd_;
};
struct __timer_task : __generic_timer_task {
    using __clock      = std::chrono::steady_clock;
    using __time_point = __clock::time_point;

    struct __time_sequence {
        __time_point __deadline_{};
        std::size_t  __seq_{};

        friend auto operator<(
            __time_sequence const &__a, __time_sequence const &__b) noexcept
            -> bool {
            return __a.__deadline_ < __b.__deadline_ ||
                   (!(__b.__deadline_ < __a.__deadline_) &&
                    __a.__seq_ < __b.__seq_);
        }
    };
    __timer_task   *__prev_  = nullptr;
    __timer_task   *__left_  = nullptr;
    __timer_task   *__right_ = nullptr;
    __time_sequence __key_{};
    void (*__fire_)(__timer_task *) noexcept = nullptr;
};
struct __stop_timer_task : __generic_timer_task {};

using __timer_heap = __intrusive_heap<
    &__timer_task::__key_, &__timer_task::__prev_, &__timer_task::__left_,
    &__timer_task::__right_>;

} // namespace __ngr::inline __v0::__core
