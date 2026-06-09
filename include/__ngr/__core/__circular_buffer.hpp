#pragma once

#include <__concepts/semiregular.h>
#include <asm-generic/types.h>
#include <cassert>

namespace __ngr::inline __v0::__core {

template <std::semiregular _Ty, __u32 _Np> struct __circular_buffer {
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline auto _M_push_back() noexcept -> _Ty & {
        return *_M_data(__tail_++);
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline auto _M_data(__u32 __n) noexcept -> _Ty * {
        static_assert((_Np & (_Np - 1)) == 0U);
        return reinterpret_cast<_Ty *>(__storage_) + (__n & (_Np - 1U));
    }

    struct __monotonic_prefix_subrange {
        [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
        inline void _M_pop_front() noexcept {
            ++__buf_->__head_;
        }
        [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]] //
        inline auto _M_front() noexcept -> _Ty & {
            return *__buf_->_M_data(__buf_->__head_);
        }
        [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]] //
        inline auto _M_empty() const noexcept -> bool {
            return __buf_->__head_ == __end_;
        }

        __circular_buffer *__buf_ = nullptr;
        __u32              __end_;
    };
    template <typename _Fn> [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline auto _M_consume_front_while(_Fn __fn) noexcept -> __monotonic_prefix_subrange {
        __u32 __h = __head_;
        while (__h != __tail_ && __fn(*_M_data(__h))) { ++__h; }
        return {this, __h};
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline auto _M_consume_swap() noexcept -> __monotonic_prefix_subrange {
        assert(__head_ != __tail_);
        return {this, __tail_};
    }

    alignas(_Ty) _Ty __storage_[_Np]{};
    __u32 __head_{};
    __u32 __tail_{};
};

} // namespace __ngr::inline __v0::__core
