#pragma once

#include <__cstddef/size_t.h>
#include <__utility/exchange.h>
#include <sys/mman.h>

namespace __ngr::inline __v0::__core {
struct __memory_mapped_region {
    void       *__data_;
    std::size_t __size_;

    __memory_mapped_region() noexcept : __data_(nullptr), __size_(0) {}
    __memory_mapped_region(void *__ptr, std::size_t __size) noexcept;
    ~__memory_mapped_region() noexcept;

    __memory_mapped_region(__memory_mapped_region const &) = delete;
    auto operator=(__memory_mapped_region const &)
        -> __memory_mapped_region & = delete;

    __memory_mapped_region(__memory_mapped_region &&__other) noexcept;

    auto operator=(__memory_mapped_region &&__other) noexcept
        -> __memory_mapped_region &;

    [[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]
    inline explicit operator bool() const noexcept;
};
inline __memory_mapped_region::__memory_mapped_region(
    void *__ptr, std::size_t __size) noexcept
    : __data_(__ptr), __size_(__size) {
    if (__data_ == MAP_FAILED) { __data_ = nullptr; }
}

inline __memory_mapped_region::~__memory_mapped_region() noexcept {
    if (__data_ != nullptr) { ::munmap(__data_, __size_); }
}

inline __memory_mapped_region::__memory_mapped_region(
    __memory_mapped_region &&__other) noexcept
    : __data_(std::exchange(__other.__data_, nullptr)),
      __size_(std::exchange(__other.__size_, 0)) {}

inline auto
__memory_mapped_region::operator=(__memory_mapped_region &&__other) noexcept
    -> __memory_mapped_region & {
    if (this != &__other) {
        if (__data_ != nullptr) { ::munmap(__data_, __size_); }
        __data_ = std::exchange(__other.__data_, nullptr);
        __size_ = std::exchange(__other.__size_, 0);
    }
    return *this;
}

inline __memory_mapped_region::operator bool() const noexcept {
    return __data_ != nullptr;
}
} // namespace __ngr::inline __v0::__core
