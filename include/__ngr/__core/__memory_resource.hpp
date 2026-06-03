#pragma once

// io_uring-aware page-aligned buffer pool
//
// Design overview
// ───────────────
// Three-tier architecture inspired by tcmalloc / brpc IOBuf:
//
//   ┌─────────────────────────────────────────┐
//   │  __buf_pool  (central, mutex-guarded)   │  owns slabs, io_uring state
//   │  global freelist per size class         │
//   └──────────────┬──────────────────────────┘
//        batch (≤32) ↕ flush / fetch
//   ┌──────────────┴──────────────────────────┐
//   │  __tl_buf_cache  (thread-local)         │  no locks, hot path
//   │  per-class intrusive freelist           │
//   └──────────────┬──────────────────────────┘
//        __buf_ref ↕ acquire / release
//   ┌──────────────┴──────────────────────────┐
//   │  __buf_meta  (one per buffer, 64 B)     │  atomic refcnt, bid, sc, data*
//   └─────────────────────────────────────────┘
//
// Page alignment
// ─────────────
// All data pages are allocated with mmap(MAP_ANONYMOUS|MAP_PRIVATE).
// sc 9 (2 MB) uses MAP_HUGETLB for transparent hugepage backing.
// The data pointer inside __buf_meta is always at a multiple of PAGE_SIZE;
// io_uring_register_buffers pins these pages in kernel page tables once.
//
// Zero-copy send (SEND_ZC) ownership model (folly IOBuf-style)
// ─────────────────────────────────────────────────────────────
//  1. __buf_acquire(sz) → __buf_ref (refcnt = 1)
//  2. fill buf._M_data(), submit IORING_OP_SEND_ZC with the fixed-buf index
//  3. first CQE: if IORING_CQE_F_MORE is set, keep the __buf_ref alive
//     (DMA still in progress); if not set, kernel copied — release now.
//  4. second CQE: IORING_CQE_F_NOTIF → call buf._M_notify() → decref → pool
//
// Provided-buffer recv (IOSQE_BUFFER_SELECT)
// ───────────────────────────────────────────
//  1. __buf_pool::_M_setup_buf_ring() registers the buf ring with the kernel
//  2. __buf_pool::_M_replenish_buf_ring() tops up the ring from TLS cache
//  3. recv CQE returns bid via (cqe.flags >> IORING_CQE_BUFFER_SHIFT) & 0xffff
//  4. pool._M_meta_of(bid) retrieves the associated __buf_meta*
//  5. after processing: __buf_pool::_M_recycle_recv_buf(bid) → TLS → replenish

#include <__atomic/atomic.h>
#include <__cstddef/size_t.h>
#include <__mutex/lock_guard.h>
#include <__mutex/mutex.h>
#include <__new/global_new_delete.h>
#include <__utility/exchange.h>
#include <cassert>
#include <cstring>
#include <liburing.h>
#include <new>
#include <sys/mman.h>

#include <__ngr/__core/__memory_mapped_region.hpp>
#include <__ngr/__core/__round_up.hpp>
#include <unistd.h>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(*-vararg)
namespace __ngr::inline __v0::__core {

// ── Size-class table
// ────────────────────────────────────────────────────────── buf_size   :
// usable data bytes per buffer (page-aligned) slab_n     : buffers allocated
// per mmap call (amortises mmap overhead) tls_max    : TLS freelist high-water
// mark before spilling a batch to global batch      : number of bufs moved TLS
// ↔ global in one critical section
//
// Batch sizes track tcmalloc's num_objects_to_move for 4 KB–8 KB objects
// (16/8), scaled down for larger sizes where fewer bufs fit in a slab.

inline constexpr int __k_n_sc_ = 10;

struct __buf_sc_entry {
    std::size_t __buf_size_;
    int         __slab_n_;
    int         __tls_max_;
    int         __batch_;
};

inline constexpr __buf_sc_entry __k_sc_table_[__k_n_sc_] = {
    // sc   buf_size      slab_n  tls_max  batch
    {   4096, 64, 32, 16}, // 0   4 KB
    {   8192, 32, 16,  8}, // 1   8 KB
    {  16384, 16,  8,  4}, // 2  16 KB
    {  32768, 16,  8,  4}, // 3  32 KB
    {  65536,  8,  4,  2}, // 4  64 KB
    { 131072,  8,  4,  2}, // 5 128 KB
    { 262144,  4,  2,  1}, // 6 256 KB
    { 524288,  4,  2,  1}, // 7 512 KB
    {1048576,  2,  2,  1}, // 8   1 MB
    {2097152,  1,  1,  1}, // 9   2 MB  (MAP_HUGETLB)
};

// Smallest size class whose buf_size ≥ __sz; returns -1 if __sz > 2 MB.
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline constexpr auto __buf_sc_of(const std::size_t __sz) noexcept -> int {
    for (int __i = 0; __i < __k_n_sc_; ++__i) {
        if (__k_sc_table_[__i].__buf_size_ >= __sz) { return __i; }
    }
    return -1;
}

// Maximum registered / provided-buffer count.  Increase if the workload needs
// more than 4 096 simultaneously pinned buffers.
inline constexpr __u32 __k_max_reg_bufs_ = 4096;

// ── Per-buffer metadata
// ──────────────────────────────────────────────────────── Lives in a
// heap-allocated array owned by __buf_slab. Each instance is exactly one cache
// line (64 B) to avoid false sharing on concurrent refcnt operations from
// producer / consumer threads.

struct __buf_pool;

struct alignas(__GCC_DESTRUCTIVE_SIZE) __buf_meta {
    std::atomic<int> __refcnt_{0}; // 0 = free; ≥ 1 = in use
    __buf_pool      *__pool_{};    // owning pool (for TLS → global flush path)
    char            *__data_{};    // page-aligned data pointer
    __u32            __bid_{};     // fixed buffer / buf-ring slot index
    __u16            __sc_{};      // size class index into __k_sc_table_
    __u16            __pad_{};
    __buf_meta      *__next_{};    // intrusive freelist link

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto _M_data() const noexcept -> char * {
        return __data_;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto _M_size() const noexcept -> std::size_t {
        return __k_sc_table_[__sc_].__buf_size_;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline void _M_incref() noexcept {
        __refcnt_.fetch_add(1, std::memory_order_relaxed);
    }

    // Returns true when the count reaches 0 — caller must return buf to pool.
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto _M_decref() noexcept -> bool {
        return __refcnt_.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }
};
static_assert(
    sizeof(__buf_meta) <= 64, "__buf_meta must fit in one cache line");

// ── Slab
// ────────────────────────────────────────────────────────────────────── One
// mmap region + a parallel array of __buf_meta. Data pages are kept separate
// from metadata so the registered iovec array can point at purely page-aligned,
// contiguous data with no embedded headers (unlike brpc's 8 KB
// block-with-header layout).

struct __buf_slab {
    __memory_mapped_region __mr_{};   // page-aligned data: n * buf_size bytes
    __buf_meta            *__meta_{}; // heap array of n __buf_meta entries
    int                    __n_{};    // number of buffers in this slab
    int                    __sc_{};   // size class

    __buf_slab() noexcept = default;

    __buf_slab(__buf_slab &&__o) noexcept
        : __mr_(std::move(__o.__mr_)),
          __meta_(std::exchange(__o.__meta_, nullptr)),
          __n_(std::exchange(__o.__n_, 0)), __sc_(std::exchange(__o.__sc_, 0)) {
    }

    __buf_slab(const __buf_slab &)                = delete;
    auto operator=(__buf_slab &&) -> __buf_slab & = delete;

    ~__buf_slab() noexcept { ::operator delete[](__meta_, std::nothrow); }

    // Allocate and initialise a new slab for size class __sci, assigning buffer
    // IDs starting at __bid_base.  Returns false on mmap/alloc failure.
    auto _M_init(int __sci, __u32 __bid_base, __buf_pool *__pool) noexcept
        -> bool {
        const __buf_sc_entry &__e = __k_sc_table_[__sci];
        const std::size_t     __tot =
            static_cast<std::size_t>(__e.__slab_n_) * __e.__buf_size_;

        int __flags = MAP_PRIVATE | MAP_ANONYMOUS;
        if (__sci == __k_n_sc_ - 1) { __flags |= MAP_HUGETLB; }

        void *__ptr =
            ::mmap(nullptr, __tot, PROT_READ | PROT_WRITE, __flags, -1, 0);
        if (__ptr == MAP_FAILED) {
            // Hugepage fallback: retry without MAP_HUGETLB.
            if (__sci == __k_n_sc_ - 1) {
                __flags &= ~MAP_HUGETLB;
                __ptr = ::mmap(
                    nullptr, __tot, PROT_READ | PROT_WRITE, __flags, -1, 0);
            }
            if (__ptr == MAP_FAILED) { return false; }
        }

        __mr_   = __memory_mapped_region{__ptr, __tot};
        __meta_ = static_cast<__buf_meta *>(
            ::operator new[](__e.__slab_n_ * sizeof(__buf_meta), std::nothrow));
        if (__meta_ == nullptr) { return false; }

        auto *__base = static_cast<char *>(__ptr);
        for (int __i = 0; __i < __e.__slab_n_; ++__i) {
            new (&__meta_[__i]) __buf_meta{};
            __meta_[__i].__pool_ = __pool;
            __meta_[__i].__data_ =
                __base + static_cast<std::ptrdiff_t>(__i) *
                             static_cast<std::ptrdiff_t>(__e.__buf_size_);
            __meta_[__i].__bid_ = __bid_base + static_cast<__u32>(__i);
            __meta_[__i].__sc_  = static_cast<__u16>(__sci);
        }

        __n_  = __e.__slab_n_;
        __sc_ = __sci;
        return true;
    }
};

// ── Central pool
// ──────────────────────────────────────────────────────────────

struct __buf_pool {
    // ── Global freelist per size class (mutex-guarded) ──
    std::mutex  __mu_{};
    __buf_meta *__global_[__k_n_sc_]   = {};
    int         __n_global_[__k_n_sc_] = {};

    // ── Slab ownership (intrusive linked list) ──
    struct __slab_node {
        __buf_slab   __slab_{};
        __slab_node *__next_{};
    };
    __slab_node *__slabs_head_{};
    __u32        __next_bid_{}; // monotonically increasing bid counter

    // ── io_uring fixed-buffer registration ──
    // Supports IORING_OP_SEND_ZC with buf_index (pre-pinned pages).
    // Register once with io_uring_register_buffers_sparse / update_tag;
    // thereafter every SEND_ZC SQE references a buf by fixed index (bid).
    ::iovec     __reg_iov_[__k_max_reg_bufs_]{};
    __buf_meta *__bid_to_meta_[__k_max_reg_bufs_]{};
    bool        __fixed_registered_{};

    // ── io_uring provided-buffer ring (IOSQE_BUFFER_SELECT, recv) ──
    // Kernel-visible ring: mmap'd struct io_uring_buf_ring.
    // Replenish after each recv CQE batch to keep the ring full.
    __memory_mapped_region __buf_ring_mr_{};
    ::io_uring_buf_ring   *__buf_ring_{};
    __u32                  __buf_ring_entries_{}; // power-of-2
    __u16                  __bgid_{};             // buffer group ID

    __buf_pool() noexcept = default;

    ~__buf_pool() noexcept {
        for (__slab_node *__n = __slabs_head_; __n != nullptr;) {
            __slab_node *__tmp = __n->__next_;
            __n->~__slab_node();
            ::operator delete(__n, std::nothrow);
            __n = __tmp;
        }
    }

    // ── Acquire / release (called by __tl_buf_cache) ──────────────────────

    // Move up to __n free bufs of class __sci from the global pool into __dst.
    // Returns the number actually moved.  Called with no lock held; acquires
    // and releases __mu_ internally.
    auto _M_fetch(int __sci, __buf_meta **__dst, int __n) noexcept -> int {
        std::lock_guard __lock{__mu_};
        int             __got = 0;
        while (__got < __n && __global_[__sci] != nullptr) {
            __buf_meta *__m  = __global_[__sci];
            __global_[__sci] = __m->__next_;
            __m->__next_     = nullptr;
            __dst[__got++]   = __m;
            --__n_global_[__sci];
        }
        if (__got == 0) {
            // Global empty: grow by one slab (still under the lock).
            if (_M_grow(__sci)) { return _M_fetch_nolock(__sci, __dst, __n); }
        }
        return __got;
    }

    // Return __n bufs to the global freelist.
    void _M_flush(int __sci, __buf_meta **__src, int __n) noexcept {
        std::lock_guard __lock{__mu_};
        for (int __i = 0; __i < __n; ++__i) {
            __src[__i]->__next_ = __global_[__sci];
            __global_[__sci]    = __src[__i];
            ++__n_global_[__sci];
        }
    }

    // Lookup metadata by buffer ID (O(1)).
    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline auto _M_meta_of(const __u32 __bid) noexcept -> __buf_meta * {
        assert(__bid < __k_max_reg_bufs_);
        return __bid_to_meta_[__bid];
    }

    // ── io_uring fixed-buffer registration ────────────────────────────────
    //
    // Call once, after all initial slabs are grown, to pin the backing pages in
    // the kernel's fixed-buffer table.  Uses the sparse registration path
    // (IORING_REGISTER_BUFFERS_SPARSE, kernel ≥ 5.13) so the table can be
    // grown later with update_tag without re-pinning existing entries.
    //
    // Returns 0 on success, -errno on failure.
    auto _M_register_fixed_bufs(const int __ring_fd) noexcept -> int {
        if (__next_bid_ == 0) { return 0; }
        // 1. Allocate sparse table with __k_max_reg_bufs_ slots.
        int __rc = static_cast<int>(::syscall(
            __NR_io_uring_register, __ring_fd,
            IORING_REGISTER_BUFFERS_UPDATE, /*IORING_REGISTER_BUFFERS_SPARSE*/
            nullptr, __k_max_reg_bufs_));
        if (__rc < 0) { return -errno; }
        // 2. Fill the populated slots [0, __next_bid_).
        struct __reg_arg {
            __u64 ptr;
            __u32 nr;
            __u32 shift;
        } __arg{
            .ptr   = reinterpret_cast<__u64>(__reg_iov_),
            .nr    = __next_bid_,
            .shift = 0,
        };
        __rc = static_cast<int>(::syscall(
            __NR_io_uring_register, __ring_fd, IORING_REGISTER_BUFFERS_UPDATE,
            &__arg, 1));
        if (__rc < 0) { return -errno; }
        __fixed_registered_ = true;
        return 0;
    }

    // Dynamically register one additional buffer slot after a slab grow.
    // Kernel ≥ 5.13 only; nop if the table has not been initialised yet.
    auto _M_update_fixed_buf(const int __ring_fd, const __u32 __bid) noexcept
        -> int {
        if (!__fixed_registered_) { return 0; }
        struct __reg_arg {
            __u64 ptr;
            __u32 nr;
            __u32 shift;
        } __arg{
            .ptr   = reinterpret_cast<__u64>(&__reg_iov_[__bid]),
            .nr    = 1,
            .shift = __bid,
        };
        int __rc = static_cast<int>(::syscall(
            __NR_io_uring_register, __ring_fd, IORING_REGISTER_BUFFERS_UPDATE,
            &__arg, 1));
        return (__rc < 0) ? -errno : 0;
    }

    // ── io_uring provided-buffer ring (IOSQE_BUFFER_SELECT) ──────────────
    //
    // Sets up a buf ring that the kernel draws from when a recv SQE carries
    // IOSQE_BUFFER_SELECT with the matching buffer group id (__bgid).
    // __entries must be a power of 2 and ≤ 32768.
    //
    // Returns 0 on success, -errno on failure.
    auto _M_setup_buf_ring(
        const int __ring_fd, const __u16 __bgid_arg,
        const __u32 __entries) noexcept -> int {
        assert((__entries & (__entries - 1)) == 0); // power-of-2
        assert(__entries <= 32768);

        // The buf ring must itself be page-aligned (mmap'd, not heap).
        const std::size_t __sz =
            __round_up(__entries * sizeof(::io_uring_buf), 4096UL);
        void *__ptr = ::mmap(
            nullptr, __sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
        if (__ptr == MAP_FAILED) { return -errno; }
        ::memset(__ptr, 0, __sz);

        ::io_uring_buf_reg __reg{};
        __reg.ring_addr    = reinterpret_cast<__u64>(__ptr);
        __reg.ring_entries = __entries;
        __reg.bgid         = __bgid_arg;

        int __rc = static_cast<int>(::syscall(
            __NR_io_uring_register, __ring_fd, IORING_REGISTER_PBUF_RING,
            &__reg, 1));
        if (__rc < 0) {
            ::munmap(__ptr, __sz);
            return -errno;
        }

        __buf_ring_mr_      = __memory_mapped_region{__ptr, __sz};
        __buf_ring_         = static_cast<::io_uring_buf_ring *>(__ptr);
        __buf_ring_entries_ = __entries;
        __bgid_             = __bgid_arg;
        return 0;
    }

    // Add up to __n buffers of size class __sci to the provided-buf ring.
    // Buffers are drawn from the calling thread's TLS cache.
    // Returns the number added.  Safe to call from the scheduler's single
    // event-loop thread only (no locking on the ring tail pointer).
    auto _M_replenish_buf_ring(const int __sci, const int __n) noexcept -> int;
    // Called when a recv CQE with IOSQE_BUFFER_SELECT has been fully consumed.
    // Returns the buffer to the TLS cache and replenishes the ring by one.
    void _M_recycle_recv_buf(const __u32 __bid) noexcept;

  private:
    // Grow the global freelist for __sci by one slab.  Called under __mu_.
    // Returns true on success.
    auto _M_grow(const int __sci) noexcept -> bool {
        if (__next_bid_ + static_cast<__u32>(__k_sc_table_[__sci].__slab_n_) >
            __k_max_reg_bufs_) [[__unlikely__]] {
            return false; // registered-buffer table full
        }
        auto *__node = static_cast<__slab_node *>(
            ::operator new(sizeof(__slab_node), std::nothrow));
        if (__node == nullptr) [[__unlikely__]] { return false; }
        new (__node) __slab_node{};

        const __u32 __base = __next_bid_;
        if (!__node->__slab_._M_init(__sci, __base, this)) {
            __node->~__slab_node();
            ::operator delete(__node, std::nothrow);
            return false;
        }

        const int __n = __node->__slab_.__n_;
        for (int __i = 0; __i < __n; ++__i) {
            __buf_meta *__m = &__node->__slab_.__meta_[__i];
            // Populate registration tables.
            __bid_to_meta_[__base + static_cast<__u32>(__i)] = __m;
            __reg_iov_[__base + static_cast<__u32>(__i)]     = ::iovec{
                .iov_base = __m->__data_,
                .iov_len  = __m->_M_size(),
            };
            // Add to global freelist.
            __m->__next_     = __global_[__sci];
            __global_[__sci] = __m;
            ++__n_global_[__sci];
        }
        __next_bid_ += static_cast<__u32>(__n);

        __node->__next_ = __slabs_head_;
        __slabs_head_   = __node;
        return true;
    }

    // Internal fetch called while __mu_ is already held (after _M_grow).
    auto
    _M_fetch_nolock(const int __sci, __buf_meta **__dst, const int __n) noexcept
        -> int {
        int __got = 0;
        while (__got < __n && __global_[__sci] != nullptr) {
            __buf_meta *__m  = __global_[__sci];
            __global_[__sci] = __m->__next_;
            __m->__next_     = nullptr;
            __dst[__got++]   = __m;
            --__n_global_[__sci];
        }
        return __got;
    }
};

// ── Thread-local cache
// ──────────────────────────────────────────────────────── Hot path:
// per-size-class intrusive freelist, no locks.
//
// Acquire:  if TLS non-empty, pop one; else fetch a batch from global pool.
// Release:  push to TLS; if TLS exceeds tls_max, flush one batch to global.
//
// The __pool_ pointer must be set before any acquire/release (typically during
// scheduler initialisation: __tl_buf_cache_.__pool_ = &scheduler.__pool_).

struct __tl_buf_cache {
    __buf_pool *__pool_{};
    __buf_meta *__free_[__k_n_sc_] = {};
    int         __n_[__k_n_sc_]    = {};

    [[__nodiscard__]]
    auto _M_acquire(const int __sci) noexcept -> __buf_meta * {
        assert(__pool_ != nullptr);
        if (__n_[__sci] > 0) {
            __buf_meta *__m = __free_[__sci];
            __free_[__sci]  = __m->__next_;
            __m->__next_    = nullptr;
            --__n_[__sci];
            return __m;
        }
        // TLS empty: fetch one batch from the central pool.
        __buf_meta *__tmp[32]; // NOLINT — max batch ≤ 32
        const int   __batch = __k_sc_table_[__sci].__batch_;
        const int   __got   = __pool_->_M_fetch(__sci, __tmp, __batch);
        if (__got == 0) [[__unlikely__]] { return nullptr; }
        // Push [1, got) into TLS freelist; return [0].
        for (int __i = 1; __i < __got; ++__i) {
            __tmp[__i]->__next_ = __free_[__sci];
            __free_[__sci]      = __tmp[__i];
            ++__n_[__sci];
        }
        return __tmp[0];
    }

    void _M_release(__buf_meta *__m) noexcept {
        assert(__pool_ != nullptr);
        const int __sci   = __m->__sc_;
        const int __tmax  = __k_sc_table_[__sci].__tls_max_;
        const int __batch = __k_sc_table_[__sci].__batch_;

        __m->__next_   = __free_[__sci];
        __free_[__sci] = __m;
        ++__n_[__sci];

        if (__n_[__sci] > __tmax) {
            // High-water: spill one batch to the central pool (brpc-style).
            __buf_meta *__tmp[32]; // NOLINT
            for (int __i = 0; __i < __batch; ++__i) {
                __buf_meta *__top = __free_[__sci];
                __free_[__sci]    = __top->__next_;
                __top->__next_    = nullptr;
                __tmp[__i]        = __top;
                --__n_[__sci];
            }
            __pool_->_M_flush(__sci, __tmp, __batch);
        }
    }
};
inline auto
__buf_pool::_M_replenish_buf_ring(const int __sci, const int __n) noexcept
    -> int {
    if (__buf_ring_ == nullptr) [[__unlikely__]] { return 0; }
    const int __mask = static_cast<int>(__buf_ring_entries_) - 1;
    extern thread_local __tl_buf_cache __tl_buf_cache_;
    int                                __added = 0;
    for (int __i = 0; __i < __n; ++__i) {
        __buf_meta *__m = __tl_buf_cache_._M_acquire(__sci);
        if (__m == nullptr) [[__unlikely__]] { break; }
        __m->__refcnt_.store(1, std::memory_order_relaxed);
        ::io_uring_buf_ring_add(
            __buf_ring_, static_cast<void *>(__m->__data_),
            static_cast<unsigned int>(__m->_M_size()),
            static_cast<unsigned short>(__m->__bid_), __mask, __added);
        ++__added;
    }
    if (__added > 0) { ::io_uring_buf_ring_advance(__buf_ring_, __added); }
    return __added;
}

inline void __buf_pool::_M_recycle_recv_buf(const __u32 __bid) noexcept {
    __buf_meta *__m = _M_meta_of(__bid);
    assert(__m != nullptr);
    if (__m->_M_decref()) {
        extern thread_local __tl_buf_cache __tl_buf_cache_;
        __tl_buf_cache_._M_release(__m);
        // Re-add this one buffer back to the ring immediately.
        if (__buf_ring_ != nullptr) {
            const int __mask = static_cast<int>(__buf_ring_entries_) - 1;
            __m->__refcnt_.store(1, std::memory_order_relaxed);
            ::io_uring_buf_ring_add(
                __buf_ring_, static_cast<void *>(__m->__data_),
                static_cast<unsigned int>(__m->_M_size()),
                static_cast<unsigned short>(__bid), __mask, 0);
            ::io_uring_buf_ring_advance(__buf_ring_, 1);
        }
    }
}

inline thread_local __tl_buf_cache __tl_buf_cache_{};

// ── RAII reference-counted buffer handle ─────────────────────────────────────
// Analogous to folly::IOBuf's external-ownership model.
//
//  - Copy  : shares ownership (incref) — zero-copy for e.g. multicast send
//  - Move  : transfers ownership without touching the counter
//  - _M_notify() : release triggered by IORING_CQE_F_NOTIF for SEND_ZC
//
// Marked [[nodiscard]] so callers cannot silently drop the only reference and
// trigger an unintended immediate return-to-pool.

struct [[__nodiscard__]] __buf_ref {
    __buf_meta *__m_{};

    constexpr __buf_ref() noexcept = default;

    // Wraps a meta for which the caller has already set refcnt = 1.
    // Does NOT increment the counter (use this after __buf_acquire).
    explicit __buf_ref(__buf_meta *__m) noexcept : __m_(__m) {}

    // Share: bump refcnt (folly IOBuf clone semantics).
    __buf_ref(const __buf_ref &__o) noexcept : __m_(__o.__m_) {
        if (__m_) [[__likely__]] { __m_->_M_incref(); }
    }

    __buf_ref(__buf_ref &&__o) noexcept
        : __m_(std::exchange(__o.__m_, nullptr)) {}

    auto operator=(const __buf_ref &__o) noexcept -> __buf_ref & {
        if (__m_ != __o.__m_) {
            _M_reset();
            __m_ = __o.__m_;
            if (__m_) [[__likely__]] { __m_->_M_incref(); }
        }
        return *this;
    }

    auto operator=(__buf_ref &&__o) noexcept -> __buf_ref & {
        if (this != &__o) {
            _M_reset();
            __m_ = std::exchange(__o.__m_, nullptr);
        }
        return *this;
    }

    ~__buf_ref() noexcept { _M_reset(); }

    [[__nodiscard__]] explicit operator bool() const noexcept {
        return __m_ != nullptr;
    }

    [[__nodiscard__]] auto _M_data() const noexcept -> char * {
        assert(__m_ != nullptr);
        return __m_->_M_data();
    }

    [[__nodiscard__]] auto _M_size() const noexcept -> std::size_t {
        assert(__m_ != nullptr);
        return __m_->_M_size();
    }

    // io_uring fixed-buffer index for IORING_OP_SEND_ZC:
    //   sqe->buf_index = buf._M_bid()
    [[__nodiscard__]] auto _M_bid() const noexcept -> __u32 {
        assert(__m_ != nullptr);
        return __m_->__bid_;
    }

    [[__nodiscard__]] auto _M_sc() const noexcept -> int {
        assert(__m_ != nullptr);
        return __m_->__sc_;
    }

    // Call when the second CQE for a SEND_ZC operation arrives with
    // IORING_CQE_F_NOTIF set — the kernel has finished DMA and the buffer is
    // safe to reuse.  Releases this reference (may return buf to pool).
    void _M_notify() noexcept { _M_reset(); }

    void _M_reset() noexcept {
        if (__m_ != nullptr && __m_->_M_decref()) {
            __tl_buf_cache_._M_release(__m_);
        }
        __m_ = nullptr;
    }
};

// ── Public acquisition helper
// ───────────────────────────────────────────────── Obtain a buffer of at least
// __sz bytes from the calling thread's TLS cache (which lazily fetches from the
// central pool on cache miss). Returns an empty __buf_ref on OOM or if __sz > 2
// MB.

[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __buf_acquire(const std::size_t __sz) noexcept -> __buf_ref {
    const int __sci = __buf_sc_of(__sz);
    if (__sci < 0) [[__unlikely__]] { return {}; }
    __buf_meta *__m = __tl_buf_cache_._M_acquire(__sci);
    if (__m == nullptr) [[__unlikely__]] { return {}; }
    __m->__refcnt_.store(1, std::memory_order_relaxed);
    return __buf_ref{__m};
}

// Convenience: acquire a buffer of the exact size class __sci.
[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
inline auto __buf_acquire_sc(const int __sci) noexcept -> __buf_ref {
    assert(__sci >= 0 && __sci < __k_n_sc_);
    __buf_meta *__m = __tl_buf_cache_._M_acquire(__sci);
    if (__m == nullptr) [[__unlikely__]] { return {}; }
    __m->__refcnt_.store(1, std::memory_order_relaxed);
    return __buf_ref{__m};
}

} // namespace __ngr::inline __v0::__core
// NOLINTEND(*-vararg)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
