#pragma once

#include <__coroutine/coroutine_handle.h>
#include <__ngr/__core/__atomic_intrusive_queue.hpp>
#include <__ngr/__core/__coro_resume.hpp>
#include <__ngr/__core/__intrusive_queue.hpp>

namespace __ngr::inline __v0::__core {
struct __allocator;
inline thread_local __allocator *__k_tls_alloc = nullptr; // NOLINT

struct __generic_task {
    inline static thread_local void *__arg_                         = nullptr;
    inline static thread_local void (*__callback_)(void *) noexcept = nullptr;
    __extension__ union {
        std::coroutine_handle<void> __resume_ = nullptr;
        __generic_task             *__target_;
    };
    mutable __allocator *__prev_alloc_ = nullptr;
    __generic_task      *__next_       = nullptr;

    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static void
    __register(void *__arg, void (*__fn)(void *) noexcept) noexcept {
        __arg_      = __arg;
        __callback_ = __fn;
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
using __task_queue [[__gnu__::__nodebug__]] =
    __intrusive_queue<&__generic_task::__next_>;
using __atomic_task_queue [[__gnu__::__nodebug__]] =
    __atomic_intrusive_queue<&__generic_task::__next_>;

} // namespace __ngr::inline __v0::__core
