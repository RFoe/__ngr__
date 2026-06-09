#pragma once

#include <__ngr/__core/__atomic_intrusive_queue.hpp>
#include <__ngr/__core/__coro_resume.hpp>
#include <__ngr/__core/__intrusive_queue.hpp>

#include <__coroutine/coroutine_handle.h>

namespace __ngr::inline __v0::__core {
struct __allocator;
inline thread_local __allocator *__k_tls_alloc = nullptr; // NOLINT

struct __task_facade {
    inline static thread_local void *__arg_                         = nullptr;
    inline static thread_local void (*__callback_)(void *) noexcept = nullptr;
    __extension__ union {
        std::coroutine_handle<void> __resume_ = nullptr;
        __task_facade              *__target_;
    };
    mutable __allocator *__prev_alloc_ = nullptr;
    __extension__ union {
        __task_facade               *__next_ = nullptr;
        std::atomic<__task_facade *> __next_atomic_;
    };

    template <auto _Func, typename _Ty>                     //
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static void __register(_Ty *__arg) noexcept
        requires requires { (_Func)((_Ty *)nullptr); }
    {
        __arg_      = __arg;
        __callback_ = [](void *__base) noexcept -> void {
            (_Func)(static_cast<_Ty *>(__base));
        };
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline void _M_do_execute() const noexcept {
        __prev_alloc_ = std::exchange(__k_tls_alloc, nullptr);
        __coro_resume(__resume_);
        __k_tls_alloc = std::exchange(__prev_alloc_, nullptr);
        if (__callback_ != nullptr) [[__likely__]] {
            std::exchange(__callback_, nullptr)(__arg_);
        }
    }
};
using __task_queue [[__gnu__::__nodebug__]] = __intrusive_queue<&__task_facade::__next_>;
using __atomic_task_queue [[__gnu__::__nodebug__]] =
    __atomic_intrusive_queue<&__task_facade::__next_>;

} // namespace __ngr::inline __v0::__core
