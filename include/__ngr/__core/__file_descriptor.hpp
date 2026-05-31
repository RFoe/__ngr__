#pragma once
#include <__utility/exchange.h>
#include <unistd.h>

namespace __ngr::inline __v0::__core {
struct __file_descriptor {
    int __fd_ = -1;

    __file_descriptor() = default;
    explicit __file_descriptor(int __fd) noexcept : __fd_(__fd) {};

    __file_descriptor(__file_descriptor const &)                     = delete;
    auto operator=(__file_descriptor const &) -> __file_descriptor & = delete;

    __file_descriptor(__file_descriptor &&__other) noexcept
        : __fd_(std::exchange(__other.__fd_, -1)) {}

    auto operator=(__file_descriptor &&__other) noexcept
        -> __file_descriptor & {
        if (this != &__other) {
            if (__fd_ != -1) [[__likely__]] { ::close(__fd_); }
            __fd_ = std::exchange(__other.__fd_, -1);
        }
        return *this;
    }
    ~__file_descriptor() noexcept {
        if (__fd_ != -1) [[__likely__]] { ::close(__fd_); }
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline explicit operator bool() const noexcept {
        return __fd_ != -1;
    }
};

} // namespace __ngr::inline __v0::__core
