#include <__ngr/__core/__awaitable.hpp>
#include <__ngr/__core/__exit.hpp>
#include <print>

using namespace __ngr::__core; // NOLINT

namespace {

auto __coro() noexcept -> __awaitable<void> {
    std::println("__coro: co_return");
    co_return;
}

} // namespace

auto main() -> int {
    auto __c    = __coro();
    auto __h    = __c.__coro_;
    auto __exit = __exit_(
        [](__awaitable<void> &) noexcept -> void {
            std::println("destroy __c");
        },
        std::move(__c));
    __exit.await_suspend(__h);
    __exit.await_resume();
    __h.resume();
}
