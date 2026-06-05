#include <__ngr/__core/__memory_resource.hpp>

#include <atomic>
#include <cassert>
#include <print>
#include <thread>
#include <vector>

namespace {
using namespace __ngr::__v0::__core::__memrc; // NOLINT

// Single acquire: all span fields must be valid.
void test_acquire_fields() {
    __span s = __acquire(128);
    assert(s.__source_ != nullptr);
    assert(s.__length_ == 128);
    assert(s.__source_->__use_count_ >= 1);
    __release(s);
}

// Two small acquires that fit in one block must share the same source block
// and the second offset must follow the first.
void test_coalescing() {
    __span a = __acquire(64);
    __span b = __acquire(64);
    assert(a.__source_ == b.__source_);
    assert(b.__offset_ == a.__offset_ + 64);
    __release(a);
    __release(b);
}

// Acquiring exactly the remaining bytes, then one more byte, must cross to a
// new block.
void test_block_boundary() {
    __u32 remaining = 4096 - __k_local.__bump_;
    if (remaining == 0) {
        // Already at boundary: next acquire always allocates a new block.
        __span s = __acquire(1);
        assert(s.__source_ != nullptr);
        assert(s.__offset_ == 0);
        __release(s);
        return;
    }
    __span a = __acquire(remaining); // fills current block exactly
    __span b = __acquire(1);         // must spill to a new block
    std::println("a.__offset_: {}, b.__offset_: {}", a.__offset_, b.__offset_);
    assert(b.__source_ != a.__source_);
    assert(b.__offset_ == 0);
    __release(a);
    __release(b);
}

// use_count must track outstanding spans on the same block.
void test_refcount() {
    __span           a          = __acquire(32);
    __PTRDIFF_TYPE__ count_base = a.__source_->__use_count_;

    __span b = __acquire(32); // same block
    assert(b.__source_ == a.__source_);
    assert(b.__source_->__use_count_ == count_base + 1);

    __release(a);
    assert(b.__source_->__use_count_ == count_base);

    __release(b);
    // count_base was 1, so after both releases the block is recycled.
    // We do not dereference __source_ past this point.
}

// Concurrent acquire+release from many threads must not crash and every
// returned span must be valid.
void test_concurrent() {
    constexpr int            kThreads = 8;
    constexpr int            kIters   = 1000000;
    std::atomic<int>         errors{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&, i] {
            for (int j = 0; j < kIters; ++j) {
                __span s = __acquire(64);
                if (s.__source_ == nullptr || s.__length_ != 64)
                    errors.fetch_add(1, std::memory_order_relaxed);
                for (__u32 __n = 0; __n < s.__length_; ++__n) {
                    s.__source_->__data_[s.__offset_ + __n] = 'a' + i;
                }
                for (__u32 __n = 0; __n < s.__length_; ++__n) {
                    assert(s.__source_->__data_[s.__offset_ + __n] == 'a' + i);
                }
                __release(s);
            }
        });
    }
    for (auto &t : workers) t.join();
    assert(errors.load() == 0);
}

// Repeatedly exhaust the local free list to exercise remote push/pop.
void test_remote_round_trip() {
    // Acquire __K_ blocks (more than __T_) to force remote interaction when
    // releasing.
    constexpr __u32     kCount = 128; // > __K_ = 64
    std::vector<__span> spans;
    spans.reserve(kCount);
    for (__u32 i = 0; i < kCount; ++i) spans.push_back(__acquire(64));
    for (auto &s : spans) __release(s);
    // If remote push/pop is broken, this will either deadlock or crash.
}

} // namespace

auto main() -> int {
    test_acquire_fields();
    test_coalescing();
    test_block_boundary();
    test_refcount();
    test_remote_round_trip();
    test_concurrent();
    return 0;
}
