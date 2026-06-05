#pragma once

#include <__system_error/system_error.h>

namespace __ngr::inline __v0::__core {

[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline void __throw_error_code_if(const bool __cond, const int __ec) {
    if (__cond) [[__unlikely__]] {
        throw std::system_error(__ec, std::system_category());
    }
}

} // namespace __ngr::inline __v0::__core
