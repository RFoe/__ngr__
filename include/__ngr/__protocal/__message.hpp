#pragma once

#include <asm-generic/types.h>
#include <limits>

namespace __ngr::inline __v0::__protocal {

inline constexpr __u32 __u32nil_ = (std::numeric_limits<__u32>::max)();

struct __descriptor {
    __u32 __index_ = __u32nil_;
    __u32 __epoch_ = __u32nil_;
};

struct __proposal {
    struct __request {
        __u32        __epoch_;
        __descriptor __last_;
    };
    struct __reply {
        __u32 __epoch_;
        __u32 __grant_;
    };
};

struct __synchronize {
    struct __request {
        __u32        __epoch_;
        __descriptor __prev_;
        __u32        __apply_;
        __u32        __count_;
    };
    struct __reply {
        __u32 __epoch_;
        __u32 __match_;
    };
};

struct __operation {
    enum class _Ty : __u32 { _S_modify, _S_query_lease, _S_query_strict };
    struct __request {
        _Ty   __flag_;
        __u32 __cookie_;
    };
    struct __reply {
        __u32 __index_;
        __u32 __cookie_;
        __u32 __error_;
    };
};

} // namespace __ngr::inline __v0::__protocal