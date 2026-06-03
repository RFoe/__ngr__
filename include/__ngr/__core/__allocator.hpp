#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include <__new/global_new_delete.h>
#include <__new/placement_new_delete.h>
#include <__ngr/__core/__round_up.hpp>

namespace __ngr::inline __v0::__core {

struct __allocator {
    static constexpr std::size_t            __min_capacity_  = 1024;
    static constexpr std::size_t            __growth_factor_ = 2;
    inline static thread_local __allocator *__local_         = nullptr;

    struct __chunk_prefix {
        __chunk_prefix *__next_;
        char           *__limit_;
        auto            _M_start() noexcept -> char            *{
            return reinterpret_cast<char *>(this) + sizeof(__chunk_prefix);
        }
    };
    struct __frame_prefix {
        __chunk_prefix *__chunk_;
        char           *__cur_bump_;
    };
    static_assert(
        __STDCPP_DEFAULT_NEW_ALIGNMENT__ == sizeof(__chunk_prefix) &&
        __STDCPP_DEFAULT_NEW_ALIGNMENT__ == sizeof(__frame_prefix));

    __chunk_prefix *__chunk_     = nullptr;
    __chunk_prefix *__cur_chunk_ = nullptr;
    char           *__cur_bump_  = nullptr;
    std::size_t     __next_cap_  = __min_capacity_;

    auto _M_allocate_chunk(std::size_t __hint) noexcept -> __chunk_prefix * {
        std::size_t __cur_cap = __next_cap_ < __hint ? __hint : __next_cap_;
        std::size_t __n       = sizeof(__chunk_prefix) + __cur_cap;

        void *__vptr      = ::operator new(__n, std::nothrow);
        auto *__chunk     = static_cast<__chunk_prefix *>(__vptr);
        __chunk->__next_  = nullptr;
        __chunk->__limit_ = static_cast<char *>(__vptr) + __n;

        if (__cur_chunk_ == nullptr) {
            __chunk_ = __chunk;
        } else {
            __cur_chunk_->__next_ = __chunk;
        }

        __next_cap_ = __cur_cap * __growth_factor_;
        return __chunk;
    }

    explicit __allocator() noexcept = default;
    __allocator(__allocator &&)     = delete;
    ~__allocator() noexcept {
        for (__chunk_prefix *__h = __chunk_; __h != nullptr;) {
            __chunk_prefix *__tmp = __h->__next_;
            ::operator delete(__h, std::nothrow);
            __h = __tmp;
        }
    }

    auto _M_push_stack_frame(std::size_t __sz) noexcept -> void * {
        std::size_t __n = __round_up(
            sizeof(__frame_prefix) + __sz, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        if (__cur_chunk_ == nullptr) {
            __chunk_prefix *__chunk = _M_allocate_chunk(__n);
            __cur_chunk_            = __chunk;
            __cur_bump_             = __chunk->_M_start();
        }

        __chunk_prefix *__saved_cur_chunk = __cur_chunk_;
        char           *__saved_cur_bump  = __cur_bump_;

        while (true) {
            char *__r = __cur_bump_ + sizeof(__frame_prefix);
            if (__r + __n <= __cur_chunk_->__limit_) {
                ::new (__cur_bump_)
                    __frame_prefix{__saved_cur_chunk, __saved_cur_bump};
                __cur_bump_ += __n;
                return __r;
            }

            if (__cur_chunk_->__next_ != nullptr) {
                __cur_chunk_ = __cur_chunk_->__next_;
                __cur_bump_  = __cur_chunk_->_M_start();
                continue;
            }

            __chunk_prefix *__chunk = _M_allocate_chunk(__n);
            __cur_chunk_            = __chunk;
            __cur_bump_             = __chunk->_M_start();
        }
    }

    void _M_pop_stack_frame(void *__fr) noexcept {
        auto *__pr   = reinterpret_cast<__frame_prefix *>(__fr) - 1;
        __cur_chunk_ = __pr->__chunk_;
        __cur_bump_  = __pr->__cur_bump_;
    }
};

} // namespace __ngr::inline __v0::__core

#endif // FRAME_ALLOCATOR_H
