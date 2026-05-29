#pragma once

namespace __ngr::inline __v0::__core {
struct __generic_task_base {
    void (*__execute_)(__generic_task_base *) noexcept = nullptr;
    __generic_task_base *__next_                       = nullptr;
    void                 _M_execute() noexcept { (*this->__execute_)(this); }
};
} // namespace __ngr::inline __v0::__core
