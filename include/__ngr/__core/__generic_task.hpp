#pragma once

#include <__coroutine/coroutine_handle.h>
#include <__utility/exchange.h>

namespace __ngr::inline __v0::__core {
struct __allocator;
struct __promise_saved_fr_alloc_wrapper {
    __allocator *__saved_fr_alloc_ = nullptr;
};
inline thread_local __allocator *_S_local_fr_alloc = nullptr;

struct __generic_task {
    using __callback_fn                             = void (*)(void *) noexcept;
    inline static thread_local void         *__arg_ = nullptr;
    inline static thread_local __callback_fn __cb_  = nullptr;
    __extension__ union {
        std::coroutine_handle<__promise_saved_fr_alloc_wrapper> __resume_ =
            nullptr;
        __generic_task *__target_;
    };
    __generic_task *__next_ = nullptr;

    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline static void _S_register_after_suspend(
        void *__arg, void (*__fn)(void *) noexcept) noexcept {
        __arg_ = __arg;
        __cb_  = __fn;
    }
    [[__gnu__::__always_inline__, __gnu__::__artificial__]] //
    inline void _M_execute() const noexcept {
        __resume_.promise().__saved_fr_alloc_ =
            std::exchange(_S_local_fr_alloc, nullptr);
        __resume_.resume();
        _S_local_fr_alloc = __resume_.promise().__saved_fr_alloc_;
        if (__cb_ != nullptr) [[__likely__]] {
            std::exchange(__cb_, nullptr)(__arg_);
        }
    }
};

} // namespace __ngr::inline __v0::__core
