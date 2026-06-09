// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#include <__ngr/__core/__scheduler.hpp>
#include <__ngr/__core/__spawn.hpp>
#include <__ngr/__core/__task.hpp>
#include <__ngr/__core/__throw_error_code_if.hpp>

#include <arpa/inet.h>
#include <liburing.h>
#include <print>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#ifndef IORING_RECV_MULTISHOT
    #define IORING_RECV_MULTISHOT (1U << 1)
#endif
#ifndef IOU_PBUF_RING_INC
    #define IOU_PBUF_RING_INC (1U << 1)
#endif
#ifndef IORING_CQE_F_BUF_MORE
    #define IORING_CQE_F_BUF_MORE (1U << 4)
#endif

using namespace __ngr::__core; // NOLINT

namespace {

__scheduler __k_schd{1024, 0};
int         __k_accept = -1;

template <typename _Ty> auto __async_spawn(_Ty) noexcept -> __spawn_task<_Ty> {
    co_return;
}

// Lightweight non-owning view into a ring buffer slot.
// Valid only within the coroutine frame between successive co_awaits on
// __async_receive — the slot may be replenished at the next await_suspend.
struct __recv_view {
    const char *__data_;
    __u32       __len_;
};

// Multishot recv awaitable with a self-contained IOU_PBUF_RING_INC buffer ring,
// modelled after the proxy example.
//
// Memory layout (proxy style): flat pool of __nr_bufs_ * __buf_size_ bytes,
// addressed as pool[bid * buf_size].  No per-slot headers, no refcounting.
//
// IOU_PBUF_RING_INC semantics: the kernel bumps its write pointer within a slot
// across multiple recvs (IORING_CQE_F_BUF_MORE == 1 while still packing).
// __cur_offset_ tracks the matching read-side position so that each CQE yields
// the correct sub-slice.  The slot is replenished at the START of the next
// await_suspend once F_BUF_MORE clears — by then the coroutine has finished
// reading the last piece and the memory is safe to hand back to the kernel.
struct __async_receive : public __ioring_task {
    static constexpr __u32 __nr_bufs_  = 256;
    static constexpr __u32 __buf_size_ = 4096;
    static constexpr __u32 __mask_     = __nr_bufs_ - 1;

    // --- ring resources ---
    ::io_uring_buf_ring *__ring_     = nullptr;
    void                *__ring_mem_ = MAP_FAILED;
    void                *__buf_mem_  = MAP_FAILED;
    __u16                __bgid_;

    // --- per-CQE state written by __complete, read by await_resume ---
    int   __rfd_;
    bool  __multishot_live_    = false;
    __u16 __bid_               = 0;
    __u32 __offset_            = 0; // start of this recv's data within the slot
    __u32 __len_               = 0;
    bool  __buf_more_          = false;
    __u32 __cur_offset_        = 0; // mirrors kernel write pointer within active bid

    // Deferred replenishment: set when F_BUF_MORE clears; executed at the start
    // of the next await_suspend (after the coroutine has consumed the last slice).
    bool  __replenish_pending_ = false;
    __u16 __replenish_bid_     = 0;

    // --- ring helpers ---

    [[__nodiscard__]] auto _M_slot(__u16 __bid) const noexcept -> char * {
        return static_cast<char *>(__buf_mem_) + __bid * __buf_size_;
    }

    void _M_replenish(__u16 __bid) noexcept {
        __u32 __tail  = __ring_->tail;
        auto &__buf   = __ring_->bufs[__tail & __mask_];
        __buf.addr    = reinterpret_cast<__u64>(_M_slot(__bid));
        __buf.len     = __buf_size_;
        __buf.bid     = __bid;
        std::atomic_thread_fence(std::memory_order_release);
        __ring_->tail = __tail + 1;
    }

    void _M_setup_ring() noexcept {
        __buf_mem_ = ::mmap(nullptr, __nr_bufs_ * __buf_size_, PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        __throw_error_code_if(__buf_mem_ == MAP_FAILED, errno);

        const std::size_t __ring_sz =
            sizeof(::io_uring_buf_ring) + __nr_bufs_ * sizeof(::io_uring_buf);
        __ring_mem_ = ::mmap(nullptr, __ring_sz, PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        __throw_error_code_if(__ring_mem_ == MAP_FAILED, errno);
        __ring_       = static_cast<::io_uring_buf_ring *>(__ring_mem_);
        __ring_->tail = 0;

        ::io_uring_buf_reg __reg{};
        __reg.ring_addr    = reinterpret_cast<__u64>(__ring_);
        __reg.ring_entries = __nr_bufs_;
        __reg.bgid         = __bgid_;
        __reg.flags        = IOU_PBUF_RING_INC;
        const int __rc = static_cast<int>(::syscall(__NR_io_uring_register,
                                                    __k_schd.__ringfd_.__fd_,
                                                    IORING_REGISTER_PBUF_RING, &__reg, 1));
        __throw_error_code_if(__rc < 0, -__rc);

        for (__u32 __i = 0; __i < __nr_bufs_; ++__i) {
            auto &__buf = __ring_->bufs[__i];
            __buf.addr  = reinterpret_cast<__u64>(_M_slot(static_cast<__u16>(__i)));
            __buf.len   = __buf_size_;
            __buf.bid   = static_cast<__u16>(__i);
        }
        std::atomic_thread_fence(std::memory_order_release);
        __ring_->tail = __nr_bufs_;
    }

    // --- ioring_task vtable ---

    static void __submit(__async_receive *__op, ::io_uring_sqe &__sqe) noexcept {
        __builtin_memset(&__sqe, 0, sizeof __sqe);
        __sqe.opcode    = IORING_OP_RECV;
        __sqe.fd        = __op->__rfd_;
        __sqe.len       = 0;
        __sqe.buf_group = __op->__bgid_;
        __sqe.flags    |= IOSQE_BUFFER_SELECT;
        __sqe.ioprio   |= IORING_RECV_MULTISHOT;
        __op->__multishot_live_ = true;
    }

    static void __complete(__async_receive *__op, const ::io_uring_cqe &__cqe) noexcept {
        if (__cqe.res == -ENOBUFS) {
            // Ring exhausted — all slots are between the kernel writing and the
            // coroutine replenishing.  Resubmit; by the next io_uring_enter the
            // coroutine will have replenished at least one slot.
            __op->__multishot_live_ = false;
            __op->__cur_offset_     = 0;
            __k_schd._M_submit(__op);
            return;
        }
        if (__cqe.res <= 0) return; // EOF or error — stop recv loop
        __throw_error_code_if(__cqe.res < 0, -__cqe.res);

        const bool __buf_more = (__cqe.flags & IORING_CQE_F_BUF_MORE) != 0U;
        if ((__cqe.flags & IORING_CQE_F_MORE) == 0U) __op->__multishot_live_ = false;

        __op->__bid_      = static_cast<__u16>(__cqe.flags >> IORING_CQE_BUFFER_SHIFT);
        __op->__len_      = static_cast<__u32>(__cqe.res);
        __op->__buf_more_ = __buf_more;
        __op->__offset_   = __op->__cur_offset_; // snapshot read-side bump for await_resume

        if (__buf_more) {
            __op->__cur_offset_ += __op->__len_; // slot still packing — advance bump
        } else {
            __op->__cur_offset_        = 0; // slot exhausted — next bid starts fresh
            __op->__replenish_pending_ = true;
            __op->__replenish_bid_     = __op->__bid_;
        }
        __op->_M_do_execute();
    }

    // --- constructor / destructor ---

    explicit __async_receive(int __fd) noexcept
        : __ioring_task(_nttp_<__submit, __complete>),
          __bgid_{[] {
              static __u16 __s = 0;
              return __s++;
          }()},
          __rfd_(__fd) {
        _M_setup_ring();
    }

    ~__async_receive() noexcept {
        if (__buf_mem_ != MAP_FAILED) ::munmap(__buf_mem_, __nr_bufs_ * __buf_size_);
        if (__ring_mem_ != MAP_FAILED) {
            ::munmap(__ring_mem_,
                     sizeof(::io_uring_buf_ring) + __nr_bufs_ * sizeof(::io_uring_buf));
        }
    }

    // --- awaitable interface ---

    static auto await_ready() noexcept -> std::false_type { return {}; }

    auto await_suspend(std::coroutine_handle<> __coro) noexcept -> void {
        // The coroutine has just finished reading the previous slice.  If that
        // slice was the last one in its slot (F_BUF_MORE was false), hand the
        // slot back to the kernel now.
        if (__replenish_pending_) {
            _M_replenish(__replenish_bid_);
            __replenish_pending_ = false;
        }
        __task_facade::__resume_ = __coro;
        if (!__multishot_live_) {
            constexpr auto __op = [](__async_receive *__self) noexcept -> void {
                __k_schd._M_submit(__self);
            };
            __task_facade::__register<__op>(this);
        }
        // multishot alive: __callback_ stays null — _M_do_execute will not re-submit,
        // next CQE resumes us directly.
    }

    auto await_resume() noexcept -> __recv_view {
        return {.__data_ = _M_slot(__bid_) + __offset_, .__len_ = __len_};
    }
};

auto __receive_coro(int __rfd) noexcept -> __task<> {
    __async_receive __recv{__rfd};
    while (true) {
        auto [__data, __len] = co_await __recv;
        std::print("{}", std::string_view{__data, __len});
        std::fflush(stdout);
    }
    co_return;
}

struct __async_accept : public __ioring_task {
    ::sockaddr_storage __addr_{};
    ::socklen_t        __addrlen_ = sizeof(::sockaddr_storage);

    static void __submit(__async_accept *__op, ::io_uring_sqe &__sqe) noexcept {
        __builtin_memset(&__sqe, 0, sizeof __sqe);
        __sqe.opcode       = IORING_OP_ACCEPT;
        __sqe.fd           = __k_accept;
        __sqe.addr         = __builtin_bit_cast(__u64, &__op->__addr_);
        __sqe.off          = __builtin_bit_cast(__u64, &__op->__addrlen_);
        __sqe.accept_flags = 0;
        __sqe.ioprio       = IORING_ACCEPT_MULTISHOT;
    }

    static void __complete(__async_accept *__op, const ::io_uring_cqe &__cqe) noexcept {
        __throw_error_code_if(__cqe.res < 0, -__cqe.res);
        __async_spawn(__receive_coro(__cqe.res)).__task_->_M_do_execute();
        if ((__cqe.flags & IORING_CQE_F_MORE) == 0U) { __k_schd._M_submit(__op); }
    }

    __async_accept() noexcept : __ioring_task(_nttp_<__submit, __complete>) {}
};

} // namespace

auto main() -> int {
    __k_accept = ::socket(AF_INET, SOCK_STREAM, 0);
    int __yes  = 1;
    ::setsockopt(__k_accept, SOL_SOCKET, SO_REUSEADDR, &__yes, sizeof __yes);
    ::sockaddr_in __addr{};
    __builtin_memset(&__addr, 0, sizeof __addr);
    __addr.sin_family      = AF_INET;
    __addr.sin_port        = htons(8888);
    __addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ::bind(__k_accept, reinterpret_cast<const ::sockaddr *>(&__addr), sizeof __addr);
    ::listen(__k_accept, SOMAXCONN);

    __async_accept __accept;
    __k_schd._M_submit(&__accept);
    __k_schd._M_run_until_stop();
}
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
