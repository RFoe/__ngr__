#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(readability-implicit-bool-conversion)
// NOLINTBEGIN(performance-no-int-to-ptr)
// NOLINTBEGIN(fuchsia-default-arguments-declarations)
namespace __ngr::inline __v0::__core {

/// __stack_frame_alloc — A LIFO bump allocator for coroutine stack frames.
///
/// Design:
///   - push/pop strictly alternate: push, pop, push, pop, ...
///   - pop is O(1): a back-pointer before each frame locates its __frame_prefix
///   - pop does NOT release chunks; release() frees everything at once
///   - First chunk size is specified at construction (determined by top frame
///   operator new)
///   - Grows by allocating larger chunks (growth factor 2)
///
/// Chunk layout (grows low → high):
///   [__chunk_header | usable area .......................... ]
///
/// Within usable area, each push produces:
///   [...prev...][__frame_prefix][backptr + padding][frame bytes...]
///                                                  ^ returned ptr
///                                                  (__align-aligned)
///
struct __frame {
  private:
    struct __descriptor {
        __descriptor *__prev_chunk_;
        std::size_t   __capacity_; // usable bytes (excluding this header)
    };

    struct __frame_prefix {
        char *__prev_sp_;
        char *__prev_chunk_end_;
    };

    __descriptor *__newest_chunk_ = nullptr; // head of chunk list (for release)
    char         *__sp_           = nullptr; // current bump pointer
    char *__chunk_end_ = nullptr; // end of the chunk that __sp_ lives in

    static constexpr std::size_t __growth_factor_  = 2;
    static constexpr std::size_t __min_chunk_size_ = 256;

    [[__gnu__::__always_inline__]]
    static constexpr auto
    _S_align_up(std::size_t __val, std::size_t __align) noexcept
        -> std::size_t {
        return (__val + __align - 1) & ~(__align - 1);
    }

    [[__gnu__::__always_inline__]]
    static auto _S_align_up_ptr(const char *__ptr, std::size_t __align) noexcept
        -> char * {
        auto __v = reinterpret_cast<std::uintptr_t>(__ptr);
        __v      = (__v + __align - 1) & ~(__align - 1);
        return reinterpret_cast<char *>(__v);
    }

    static auto _S_chunk_usable_start(__descriptor *__h) noexcept -> char * {
        return reinterpret_cast<char *>(__h) + sizeof(__descriptor);
    }

    static auto _S_chunk_usable_end(__descriptor *__h) noexcept -> char * {
        return reinterpret_cast<char *>(__h) + sizeof(__descriptor) +
               __h->__capacity_;
    }

    void _M_allocate_chunk(std::size_t __min_usable) {
        std::size_t __prev_cap =
            __newest_chunk_ ? __newest_chunk_->__capacity_ : 0;
        std::size_t __new_cap = __prev_cap * __growth_factor_;
        __new_cap             = std::max(__new_cap, __min_usable);
        __new_cap             = std::max(__new_cap, __min_chunk_size_);
        __new_cap = _S_align_up(__new_cap, alignof(std::max_align_t));

        std::size_t __alloc_size = sizeof(__descriptor) + __new_cap;
        void       *__mem        = ::operator new(__alloc_size);

        auto *__hdr          = static_cast<__descriptor *>(__mem);
        __hdr->__prev_chunk_ = __newest_chunk_;
        __hdr->__capacity_   = __new_cap;

        __newest_chunk_ = __hdr;
        __sp_           = _S_chunk_usable_start(__hdr);
        __chunk_end_    = _S_chunk_usable_end(__hdr);
    }

  public:
    __frame() = default;

    __frame(const __frame &)                     = delete;
    auto operator=(const __frame &) -> __frame & = delete;

    __frame(__frame &&__o) noexcept
        : __newest_chunk_(__o.__newest_chunk_), __sp_(__o.__sp_),
          __chunk_end_(__o.__chunk_end_) {
        __o.__newest_chunk_ = nullptr;
        __o.__sp_           = nullptr;
        __o.__chunk_end_    = nullptr;
    }

    auto operator=(__frame &&__o) noexcept -> __frame & {
        if (this != &__o) {
            release();
            __newest_chunk_     = __o.__newest_chunk_;
            __sp_               = __o.__sp_;
            __chunk_end_        = __o.__chunk_end_;
            __o.__newest_chunk_ = nullptr;
            __o.__sp_           = nullptr;
            __o.__chunk_end_    = nullptr;
        }
        return *this;
    }

    ~__frame() noexcept { release(); }

    [[__nodiscard__]]
    auto _M_push(
        const std::size_t __n,
        const std::size_t __a = __STDCPP_DEFAULT_NEW_ALIGNMENT__) noexcept
        -> void * {
        assert(__a >= sizeof(void *) && (__a & (__a - 1)) == 0);

        // Snapshot current state (to be saved in the prefix).
        char *__saved_sp  = __sp_;
        char *__saved_end = __chunk_end_;

        auto __try_push = [&] noexcept -> void * {
            if (!__sp_) return nullptr;

            char *__prefix_pos   = __sp_;
            char *__after_prefix = __prefix_pos + sizeof(__frame_prefix);
            // Reserve sizeof(void*) for back-pointer, then align up for frame.
            char *__frame_start =
                _S_align_up_ptr(__after_prefix + sizeof(void *), __a);
            char *__frame_end = __frame_start + __n;

            if (__frame_end > __chunk_end_) return nullptr;

            // Write prefix (saves previous sp + chunk_end).
            auto *__prefix = reinterpret_cast<__frame_prefix *>(__prefix_pos);
            __prefix->__prev_sp_        = __saved_sp;
            __prefix->__prev_chunk_end_ = __saved_end;

            // Write back-pointer at (frame_start - sizeof(void*)).
            *reinterpret_cast<__frame_prefix **>(
                __frame_start - sizeof(void *)) = __prefix;

            __sp_ = __frame_end;
            return static_cast<void *>(__frame_start);
        };

        if (void *__p = __try_push()) { return __p; }

        // Current chunk insufficient — allocate a new one.
        const std::size_t __overhead =
            sizeof(__frame_prefix) + sizeof(void *) + __a;
        _M_allocate_chunk(__overhead + __n);

        void *__p = __try_push();
        assert(__p && "freshly allocated chunk must satisfy push");
        return __p;
    }

    /// pop — Restore state to before the corresponding push. O(1).
    /// Does NOT free any chunks (deferred to release()).
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline void _M_pop(void *__frame_ptr) noexcept {
        // Back-pointer is at (frame_ptr - sizeof(void*)).
        auto **__bp = reinterpret_cast<__frame_prefix **>(
            static_cast<char *>(__frame_ptr) - sizeof(void *));
        __frame_prefix *__prefix = *__bp;

        __sp_        = __prefix->__prev_sp_;
        __chunk_end_ = __prefix->__prev_chunk_end_;
        // __newest_chunk_ is NOT modified — it always points to the most
        // recently allocated chunk so that release() can walk the full list.
    }

    /// release — Free ALL chunks. Reset to empty state.
    void release() noexcept {
        __descriptor *__h = __newest_chunk_;
        while (__h) {
            __descriptor *__prev = __h->__prev_chunk_;
            ::operator delete(static_cast<void *>(__h));
            __h = __prev;
        }
        __newest_chunk_ = nullptr;
        __sp_           = nullptr;
        __chunk_end_    = nullptr;
    }
};

} // namespace __ngr::inline __v0::__core
// NOLINTEND(fuchsia-default-arguments-declarations)
// NOLINTEND(performance-no-int-to-ptr)
// NOLINTEND(readability-implicit-bool-conversion)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
