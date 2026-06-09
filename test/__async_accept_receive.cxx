#include <__ngr/__core/__scheduler.hpp>
#include <__ngr/__core/__spawn.hpp>
#include <__ngr/__core/__task.hpp>
#include <__ngr/__core/__throw_error_code_if.hpp>

using namespace __ngr::__core;

namespace {

__scheduler __k_sched{1024, 0};
int         __k_accept = -1;

template <typename _Ty> auto __async_spawn(_Ty) noexcept -> __spawn_task<_Ty> {
    co_return;
}

struct __async_receive : __ioring_task {
    void *__ring_mem_;
    void *__buf_mem_;

    [[__nodiscard__]] auto _M_slot(__u16 __n) const noexcept -> char * {
        return static_cast<char *>(__buf_mem_) + __n * 4096;
    }
};

} // namespace
