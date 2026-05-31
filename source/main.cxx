#include <__ngr/__core/__task.hpp>
#include <cstdio>
#include <unistd.h>

using namespace __ngr::__core;

namespace {

template <std::size_t _Np> void _S_print(const char (&__msg)[_Np]) noexcept {
    ::write(fileno(stdout), __msg, _Np);
}

auto __async_coro(int __n) -> __task<> {
    _S_print("coro\n");
    if (__n < 10) { co_await __async_coro(__n + 1); }
    if (__n < 10) { co_await __async_coro(__n + 1); }
};

} // namespace

auto main() -> int {
    _S_print("A\n");
    auto __h = __async_coro(0);
    __h.__coro_.resume();
    _S_print("B\n");
}
