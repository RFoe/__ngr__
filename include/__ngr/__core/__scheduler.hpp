// coro __task ref count
// TODO
#pragma once
#include <liburing.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <__algorithm/clamp.h>
#include <__chrono/duration.h>
#include <__chrono/steady_clock.h>
#include <__mutex/lock_guard.h>
#include <__system_error/system_error.h>

#include <__ngr/__core/__atomic_segment_array.hpp>
#include <__ngr/__core/__file_descriptor.hpp>
#include <__ngr/__core/__generic_task.hpp>
#include <__ngr/__core/__intrusive_heap.hpp>
#include <__ngr/__core/__memory_mapped_region.hpp>
#include <__ngr/__core/__stop_inplace.hpp>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(*-vararg)
namespace __ngr::inline __v0::__core {

[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline void __throw_error_code_if(const bool __cond, const int __ec) {
    if (__cond) [[__unlikely__]] {
        throw std::system_error(__ec, std::system_category());
    }
}

inline auto __io_uring_setup(unsigned __entries, ::io_uring_params &__params)
    -> __file_descriptor {
    int __rc =
        static_cast<int>(::syscall(__NR_io_uring_setup, __entries, &__params));
    __throw_error_code_if(__rc < 0, -__rc);
    return __file_descriptor{__rc};
}

inline auto __io_uring_enter(
    int __ring_fd, unsigned int __to_submit, unsigned int __min_complete,
    unsigned int __flags) -> int {
    int __rc = static_cast<int>(::syscall(
        __NR_io_uring_enter, __ring_fd, __to_submit, __min_complete, __flags,
        nullptr, 0));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __duration_to_timespec(std::chrono::nanoseconds __dur) noexcept
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
inline auto __io_uring_enter(
    int __ring_fd, unsigned int __to_submit, unsigned int __min_complete,
    unsigned int __flags, void *__arg, std::size_t __argsz) noexcept -> int {
    int __rc = static_cast<int>(::syscall(
        __NR_io_uring_enter, __ring_fd, __to_submit, __min_complete, __flags,
        __arg, __argsz));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __io_uring_register(
    int __ring_fd, unsigned __opcode, void *__arg, unsigned __nr) noexcept
    -> int {
    int __rc = static_cast<int>(
        ::syscall(__NR_io_uring_register, __ring_fd, __opcode, __arg, __nr));
    if (__rc == -1) [[__unlikely__]] { return -errno; }
    return __rc;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline void __io_uring_register_eventfd_async(int __rfd, int __efd) {
    int __rc = static_cast<int>(::syscall(
        SYS_io_uring_register, __rfd, IORING_REGISTER_EVENTFD_ASYNC, &__efd,
        1));
    __throw_error_code_if(__rc < 0, -errno);
}

[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline void __io_uring_register_napi_busy_loop(int __rfd, __u32 __us) {
    ::io_uring_napi __napi{};
    __napi.busy_poll_to     = __us;
    __napi.prefer_busy_poll = 1;
    int __rc                = static_cast<int>(::syscall(
        __NR_io_uring_register, __rfd, IORING_REGISTER_NAPI, &__napi, 1));
    __throw_error_code_if(__rc < 0, -errno);
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __map_region(int __fd, ::off_t __offset, std::size_t __size)
    -> __memory_mapped_region {
    void *__ptr = ::mmap(
        nullptr, __size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
        __fd, __offset);
    __throw_error_code_if(__ptr == MAP_FAILED, errno);
    return __memory_mapped_region{__ptr, __size};
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto __umin(__u32 __a, __u32 __b) noexcept -> __u32 {
    return __a < __b ? __a : __b;
}

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto __umax(__u32 __a, __u32 __b) noexcept -> __u32 {
    return __a < __b ? __b : __a;
}

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
template <typename _Ty>
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __at_offset_as(void *__base, __u32 __offset) noexcept -> _Ty {
    return reinterpret_cast<_Ty>(static_cast<std::byte *>(__base) + __offset);
}
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

template <typename _Ty> struct __wrapper {
    consteval __wrapper(_Ty __v) noexcept : __v_(__v) {}
    _Ty __v_;
};
template <__wrapper _Vp> struct __nttp {};
template <__wrapper _Vp> inline constexpr __nttp<_Vp> _nttp_{};

struct __io_uring_task : __generic_task {
    enum class _Sg : unsigned char { _S_submit, _S_complete };
    __extension__ union __params {
        ::io_uring_sqe       *__sqe_;
        const ::io_uring_cqe *__cqe_;
    } __attribute__((__trivial_abi__));
    void (*__virt_)(__io_uring_task *, const _Sg, __params) noexcept;
    template <__wrapper __submit_, __wrapper __complete_> static void
    __manage(__io_uring_task *__x, const _Sg __op, __params __p) noexcept {
        switch (__op) {
        case _Sg::_S_submit:   (__submit_.__v_)(__x, *__p.__sqe_); return;
        case _Sg::_S_complete: (__complete_.__v_)(__x, *__p.__cqe_); return;
        }
    }
    void _M_submit(::io_uring_sqe &__sqe) noexcept {
        __params __p{.__sqe_ = &__sqe};
        (*this->__virt_)(this, _Sg::_S_submit, __p);
    }
    void _M_complete(const ::io_uring_cqe &__cqe) noexcept {
        __params __p{.__cqe_ = &__cqe};
        (*this->__virt_)(this, _Sg::_S_complete, __p);
    }
    template <__wrapper __submit_, __wrapper __complete_>
    __io_uring_task(__nttp<__submit_>, __nttp<__complete_>) noexcept
        : __virt_(&__manage<__submit_, __complete_>) {}
};
struct __generic_timer_task : __generic_task {
    enum class _Sg : unsigned char { _S_schedule, _S_stop };
    _Sg  __cmd_;
    bool __stop_ = false;
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
    void (*__complete_)(__timer_task *) noexcept = nullptr;

    void _M_complete() noexcept { (*this->__complete_)(this); }
    void _M_stop() noexcept {
        __stop_ = true;
        (*this->__complete_)(this);
    }
};

using __timer_heap = __intrusive_heap<
    &__timer_task::__key_, &__timer_task::__prev_, &__timer_task::__left_,
    &__timer_task::__right_>;

inline void __manual_stop(__io_uring_task *__op) noexcept {
    ::io_uring_cqe __cqe{};
    __cqe.res       = -ECANCELED;
    __cqe.user_data = std::bit_cast<__u64>(__op);
    __op->_M_complete(__cqe);
}
inline void __manual_stop(__generic_timer_task *__op) noexcept {
    switch (__op->__cmd_) {
        using _Sg = __generic_timer_task::_Sg;
    case _Sg::_S_schedule: static_cast<__timer_task *>(__op)->_M_stop(); break;
    case _Sg::_S_stop:     {
        static_cast<__timer_task *>(__op->__target_)->_M_stop();
        break;
    }
    }
}

struct __submission_queue {
    std::atomic_ref<__u32> __head_;
    std::atomic_ref<__u32> __tail_;
    __u32                 *__array_;
    ::io_uring_sqe        *__sqe_;
    const __u32            __mask_;
    const __u32            __n_slot_;
    explicit __submission_queue(
        const __memory_mapped_region &__sq, const __memory_mapped_region &__sqe,
        const ::io_uring_params &__p)
        : __head_{*__at_offset_as<__u32 *>(__sq.__data_, __p.sq_off.head)},
          __tail_{*__at_offset_as<__u32 *>(__sq.__data_, __p.sq_off.tail)},
          __array_{__at_offset_as<__u32 *>(__sq.__data_, __p.sq_off.array)},
          __sqe_{static_cast<::io_uring_sqe *>(__sqe.__data_)},
          __mask_{*__at_offset_as<__u32 *>(__sq.__data_, __p.sq_off.ring_mask)},
          __n_slot_{__p.sq_entries} {}

    auto _M_submit(__task_queue &__queue, __u32 __max, bool __stop) noexcept
        -> __u32 {
        __u32 __tail          = __tail_.load(std::memory_order_relaxed);
        __u32 __head          = __head_.load(std::memory_order_acquire);
        __u32 __n             = __tail - __head;
        __u32 __r             = 0;
        __max                 = __umin(__max, __n_slot_ - __n);
        __io_uring_task *__op = nullptr;
        while (!__queue._M_empty() && __r < __max) {
            __u32           __m   = __tail & __mask_;
            ::io_uring_sqe &__sqe = __sqe_[__m];
            __op = static_cast<__io_uring_task *>(__queue._M_pop_front());
            __op->_M_submit(__sqe_[__m]);
            __stop = __stop && __sqe.opcode != IORING_OP_ASYNC_CANCEL;
            if (__stop) {
                __manual_stop(__op);
            } else {
                __sqe.user_data = std::bit_cast<__u64>(__op);
                __array_[__m]   = __m;
                ++__r;
                ++__tail;
            }
        }
        __tail_.store(__tail, std::memory_order_release);
        return __r;
    }
};

struct __completion_queue {
    std::atomic_ref<__u32> __head_;
    std::atomic_ref<__u32> __tail_;
    ::io_uring_cqe        *__cqe_;
    const __u32            __mask_;

    explicit __completion_queue(
        const __memory_mapped_region &__cqe,
        const ::io_uring_params      &__p) noexcept
        : __head_{*__at_offset_as<__u32 *>(__cqe.__data_, __p.cq_off.head)},
          __tail_{*__at_offset_as<__u32 *>(__cqe.__data_, __p.cq_off.tail)},
          __cqe_{
              __at_offset_as<::io_uring_cqe *>(__cqe.__data_, __p.cq_off.cqes)},
          __mask_{
              *__at_offset_as<__u32 *>(__cqe.__data_, __p.cq_off.ring_mask)} {}

    auto _M_complete() noexcept -> __u32 {
        __u32 __head = __head_.load(std::memory_order_relaxed);
        __u32 __tail = __tail_.load(std::memory_order_acquire);
        __u32 __n    = 0;
        while (__head != __tail) {
            __u32                 __m   = __head & __mask_;
            const ::io_uring_cqe &__cqe = __cqe_[__m];
            auto *__op = std::bit_cast<__io_uring_task *>(__cqe.user_data);
            __op->_M_complete(__cqe);
            ++__head;
            ++__n;
            __tail = __tail_.load(std::memory_order_acquire);
        }
        __head_.store(__head, std::memory_order_release);
        return __n;
    }
};
struct __scheduler_base {
    __memory_mapped_region __sq_region_{};
    __memory_mapped_region __cq_region_{};
    __memory_mapped_region __sqe_region_{};
    ::io_uring_params      __params_{};
    __file_descriptor      __ringfd_{};
    __file_descriptor      __eventfd_{};

    static auto __init_params(unsigned __flags) noexcept -> ::io_uring_params {
        ::io_uring_params __params{};
        __params.flags = __flags;
        __params.flags |= IORING_SETUP_DEFER_TASKRUN;
        __params.flags |= IORING_SETUP_SINGLE_ISSUER;
        __params.features |= IORING_FEAT_SINGLE_MMAP;
        return __params;
    }

    explicit __scheduler_base(unsigned __entries, unsigned __flags)
        : __params_{__init_params(__flags)},
          __ringfd_{__io_uring_setup(__entries, __params_)},
          __eventfd_{::eventfd(0, EFD_CLOEXEC)} {
        __throw_error_code_if(!__ringfd_, errno);
        __throw_error_code_if(!__eventfd_, errno);
        auto __n_sq =
            __params_.sq_off.array + __params_.sq_entries * sizeof(unsigned);
        auto __n_cq  = __params_.cq_off.cqes +
                       __params_.cq_entries * sizeof(::io_uring_cqe);
        auto __n_sqe = __params_.sq_entries * sizeof(::io_uring_sqe);
        if ((__params_.features & IORING_FEAT_SINGLE_MMAP) != 0U) {
            __n_sq = __umax(__n_sq, __n_cq);
            __n_cq = __n_sq;
        }
        __sq_region_ =
            __map_region(__ringfd_.__fd_, IORING_OFF_SQ_RING, __n_sq);
        __sqe_region_ = __map_region(__ringfd_.__fd_, IORING_OFF_SQES, __n_sqe);
        if (((__params_.features & IORING_FEAT_SINGLE_MMAP) == 0U)) {
            __cq_region_ =
                __map_region(__ringfd_.__fd_, IORING_OFF_CQ_RING, __n_cq);
        }
        __io_uring_register_eventfd_async(__ringfd_.__fd_, __eventfd_.__fd_);
        __io_uring_register_napi_busy_loop(__ringfd_.__fd_, 50);
    }
};
struct __scheduler : __scheduler_base {
    struct __remote_task_queue {
        __atomic_segment_array<__atomic_task_queue *, 128> __array_{};

        auto _M_pop_swap_reversed() const noexcept -> __task_queue {
            return __array_.template _M_fold_inplace<__task_queue>(
                [](auto &__init, auto *__queue) noexcept -> void {
                    return __init._M_append(__queue->_M_pop_swap_reversed());
                });
        }
    };
    enum class _Sg : unsigned char { _S_run, _S_sleep, _S_notify };
    static constexpr int __no_new_submit_ = -1;

    __completion_queue __cq_;
    __submission_queue __sq_;
    std::mutex         __mut_;

    std::atomic<_Sg> __state_              = _Sg::_S_run;
    std::atomic<int> __n_submit_in_flight_ = 0;
    std::ptrdiff_t   __n_total_submit_     = 0;
    std::ptrdiff_t   __n_newly_submit_     = 0;

    __stop_source       __source_{};
    __remote_task_queue __io_remote_{};
    __remote_task_queue __dr_remote_{};
    __timer_heap        __heap_{};
    __task_queue        __io_pending_{};
    __task_queue        __dr_pending_{};

    explicit __scheduler(unsigned __n, unsigned __flags)
        : __scheduler_base(__n, __flags),
          __cq_{__cq_region_ ? __cq_region_ : __sq_region_, __params_},
          __sq_{__sq_region_, __sqe_region_, __params_} {}

    void _M_accept_remote_request() noexcept {
        __io_pending_._M_append(__io_remote_._M_pop_swap_reversed());
        __dr_pending_._M_append(__dr_remote_._M_pop_swap_reversed());
    }

    auto _M_notify() -> bool {
        if (__state_.exchange(_Sg::_S_notify, std::memory_order_relaxed) ==
            _Sg::_S_sleep) {
            { std::lock_guard __lock{__mut_}; }
            __u64 __buf = 1;
            int   __rc  = (int)::write(__eventfd_.__fd_, &__buf, sizeof(__buf));
            __throw_error_code_if(__rc == -1, -__rc);
            return true;
        }
        return false;
    }
    void _M_request_stop() {
        __source_._M_request_stop();
        _M_notify();
    }

    void _M_io_uring_submit(bool __stop) noexcept {
        __u32 __max = __params_.cq_entries - __u32(__n_total_submit_);
        __u32 __n   = __sq_._M_submit(__io_pending_, __max, __stop);
        __n_total_submit_ += __n;
        __n_newly_submit_ += __n;
        __task_queue __tmp = std::move(__dr_pending_);
        if (__stop) {
            while (!__tmp._M_empty()) {
                __manual_stop(
                    static_cast<__generic_timer_task *>(__tmp._M_pop_front()));
            }
        } else {
            while (!__tmp._M_empty()) {
                auto *__op =
                    static_cast<__generic_timer_task *>(__tmp._M_pop_front());
                switch (__op->__cmd_) {
                    using _Sg = __generic_timer_task::_Sg;
                case _Sg::_S_schedule:
                    __heap_._M_push(static_cast<__timer_task *>(__op));
                    break;
                case _Sg::_S_stop: {
                    auto *__target =
                        static_cast<__timer_task *>(__op->__target_);
                    __heap_._M_remove(__target);
                    __target->_M_stop();
                    break;
                }
                }
            }
        }
    }

    void _M_run_some() noexcept {
        __n_total_submit_ -= __cq_._M_complete();
        _M_io_uring_submit(__source_._M_stop_requested());
    }

    void _M_submit(__io_uring_task *__op) noexcept {
        __io_pending_._M_push_front(__op);
    }

    void _M_submit(__atomic_task_queue *__request, auto *__op) {
        int __n = 0;
        while (__n != __no_new_submit_ &&
               !__n_submit_in_flight_.compare_exchange_weak(
                   __n, __n + 1, std::memory_order_acquire,
                   std::memory_order_relaxed));
        if (__n == __no_new_submit_) {
            __manual_stop(__op);
            return;
        }
        __request->_M_push_front(__op);
        __n_submit_in_flight_.fetch_sub(1, std::memory_order_relaxed);
        _M_notify();
    }

    void _M_io_uring_enter() {
        _Sg              __exp = _Sg::_S_run;
        std::unique_lock __lock{__mut_};
        if (__state_.compare_exchange_strong(
                __exp, _Sg::_S_sleep, std::memory_order_acq_rel)) {
            int               __rc __attribute__((__uninitialized__)); // NOLINT
            __kernel_timespec __ts __attribute__((__uninitialized__)); // NOLINT
            bool              __timeout = false;

            if (__timer_task *__top = __heap_.__root_) {
                __ts = __duration_to_timespec(
                    __top->__key_.__deadline_ - __timer_task::__clock::now());
                __timeout = true;
            }
            if (__timeout) {
                ::io_uring_getevents_arg __arg{};
                __arg.ts = std::bit_cast<__u64>(&__ts);
                __lock.unlock();
                __rc = __io_uring_enter(
                    __ringfd_.__fd_, __n_newly_submit_, 1,
                    IORING_ENTER_GETEVENTS | IORING_ENTER_EXT_ARG, &__arg,
                    sizeof(__arg));
            } else {
                __lock.unlock();
                __rc = __io_uring_enter(
                    __ringfd_.__fd_, __n_newly_submit_, 1,
                    IORING_ENTER_GETEVENTS);
            }
            __state_.store(_Sg::_S_run, std::memory_order_relaxed);
            __throw_error_code_if(
                __rc < 0 && __rc != -EINTR && __rc != -ETIME, -__rc);
            if (-__rc == ETIME) {
                __n_newly_submit_ = 0;
            } else if (-__rc != EINTR) [[__likely__]] {
                __n_newly_submit_ -= __rc;
            }
        }
    }

    void _M_run_until_stop() {
        _M_accept_remote_request();
        while (true) {
            _M_run_some();
            if (__n_total_submit_ == 0 ||
                (__n_total_submit_ == 1 && __source_._M_stop_requested())) {
                break;
            }
            _M_io_uring_enter();
            _M_accept_remote_request();
        }
        int __n = 0;
        while (!__n_submit_in_flight_.compare_exchange_weak(
            __n, __no_new_submit_, std::memory_order_relaxed)) {
            if (__n == __no_new_submit_) { break; }
            __n = 0;
        }
        _M_accept_remote_request();
        _M_io_uring_submit(true);
        __io_uring_enter(
            __ringfd_.__fd_, __n_newly_submit_, __n_total_submit_,
            IORING_ENTER_GETEVENTS);
        [[__maybe_unused__]] __u32 __m = __cq_._M_complete();
        assert(__n_total_submit_ == __m);
    }
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-vararg)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
