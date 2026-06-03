#pragma once

#include <__bit/bit_ceil.h>
#include <__cstddef/size_t.h>
#include <cassert>

namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline auto __path_bit_mask(const std::size_t __n) noexcept -> std::size_t {
    return std::bit_ceil(__n + 1) >> 2U;
}

template <auto _Key, auto _Prev, auto _Left, auto _Right>
struct __intrusive_heap;

template <
    typename _Node, typename _KeyT, _KeyT _Node::*_Key, _Node *_Node::*_Prev,
    _Node *_Node::*_Left, _Node *_Node::*_Right>
struct __intrusive_heap<_Key, _Prev, _Left, _Right> {
    void _M_push(_Node *__node) noexcept {
        __size_++;
        _Node **__pp = &__root_;
        _Node  *__p  = __root_;
        _Node  *__q  = nullptr;

        std::size_t __k    = __size_;
        std::size_t __mask = __path_bit_mask(__size_);
        while (__mask != 0U) {
            if (__node->*_Key < __p->*_Key) {
                __node->*_Prev = __q;
                __p->*_Prev    = __node;
                *__pp          = __node;
                do {
                    if ((__mask & __k) != 0U) {
                        __node->*_Right     = __p;
                        __node->*_Left      = __p->*_Left;
                        __p->*_Left->*_Prev = __node;
                        __node              = __p;
                        __p                 = __p->*_Right;
                    } else {
                        __node->*_Right = __p->*_Right;
                        __node->*_Left  = __p;
                        if (__p->*_Right != nullptr) [[likely]] {
                            __p->*_Right->*_Prev = __node;
                        }
                        __node = __p;
                        __p    = __p->*_Left;
                    }
                    __mask >>= 1U;
                } while (__mask != 0U);

                assert(__node->*_Right == nullptr);
                __node->*_Left = nullptr;
                return;
            }
            if ((__mask & __k) != 0U) {
                __pp = &(__p->*_Right);
            } else {
                __pp = &(__p->*_Left);
            }
            __q = __p;
            __p = *__pp;
            __mask >>= 1U;
        }

        *__pp           = __node;
        __node->*_Prev  = __q;
        __node->*_Right = nullptr;
        __node->*_Left  = nullptr;
    }

    void _M_pop() noexcept {
        if (__size_ <= 1) [[unlikely]] {
            __root_ = nullptr;
            __size_ = 0;
            return;
        }
        assert(__root_ != nullptr);
        _Node *__p   = __remove_least_leaf(&__root_, __size_);
        _Node *__top = __root_;
        __p->*_Right = __top->*_Right;
        __p->*_Left  = __top->*_Left;
        __root_      = __p;

        __size_--;
        __heap_down_sink(__root_, &__root_, nullptr);
        __root_->*_Prev = nullptr;
    }

    auto _M_remove(_Node *__node) noexcept -> bool {
        if (__node->*_Prev == nullptr && __node != __root_) { return false; }
        assert(__root_ != nullptr);
        assert(__size_ != 0);
        _Node *__p = __remove_least_leaf(&__root_, __size_);
        __size_--;
        if (__p == __node) [[unlikely]] {
            __node->*_Prev = nullptr;
            return true;
        }
        __p->*_Right = __node->*_Right;
        __p->*_Left  = __node->*_Left;
        _Node **__pp __attribute__((__uninitialized__)); // NOLINT
        _Node  *__parent = __node->*_Prev;
        if (__parent != nullptr) {
            if (__parent->*_Right == __node) {
                __pp = &(__parent->*_Right);
            } else {
                __pp = &(__parent->*_Left);
            }
            *__pp = __p;
            if (__p->*_Key < __parent->*_Key) {
                __heap_up_percolate(__p, __parent);
                __node->*_Prev = nullptr; // Make node erasure-aware.
                return true;
            }
        } else {
            __pp  = &__root_;
            *__pp = __p;
        }
        __heap_down_sink(__p, __pp, __parent);
        (*__pp)->*_Prev = __parent;
        __node->*_Prev  = nullptr;
        return true;
    }

    _Node      *__root_ = nullptr;
    std::size_t __size_ = 0;

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static void
    __swap_with_right_child(_Node *__parent, _Node *__child) noexcept {
        __parent->*_Right = __child->*_Right;
        __child->*_Right  = __parent;

        _Node *__tmp     = __parent->*_Left;
        __parent->*_Left = __child->*_Left;
        __child->*_Left  = __tmp;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static void
    __swap_with_left_child(_Node *__parent, _Node *__child) noexcept {
        _Node *__tmp      = __parent->*_Right;
        __parent->*_Right = __child->*_Right;
        __child->*_Right  = __tmp;

        __parent->*_Left = __child->*_Left;
        __child->*_Left  = __parent;
    }

    static void __heap_up_percolate(_Node *__p, _Node *__parent) noexcept {
        _Node  *__sentinel = nullptr;
        _Node **__parent_x =
            (__p->*_Left != nullptr) ? &(__p->*_Left->*_Prev) : &__sentinel;
        _Node **__parent_y =
            (__p->*_Right != nullptr) ? &(__p->*_Right->*_Prev) : &__sentinel;

        *__parent_y           = __parent;
        _Node *__grand_parent = __parent->*_Prev;
        do {
            *__parent_x = __parent;
            if (__parent->*_Right == __p) {
                __parent_x = &(__parent->*_Left->*_Prev);
                __swap_with_right_child(__parent, __p);
            } else {
                __parent_x = &(__parent->*_Right->*_Prev);
                __swap_with_left_child(__parent, __p);
            }
            __parent_y = &(__parent->*_Prev);
            if (__grand_parent->*_Right == __parent) {
                __grand_parent->*_Right = __p;
            } else {
                __grand_parent->*_Left = __p;
            }
            __parent       = __grand_parent;
            __grand_parent = __parent->*_Prev;
        } while (__p->*_Key < __parent->*_Key);
        *__parent_x = __p;
        *__parent_y = __p;
        __p->*_Prev = __parent;
    }

    static void
    __heap_down_sink(_Node *__p, _Node **__pp, _Node *__parent) noexcept {
        _Node *__r = __p->*_Right;
        _Node *__l = __p->*_Left;
        while (__l != nullptr) {
            if ((__r != nullptr) && (__r->*_Key < __l->*_Key)) {
                if (__r->*_Key < __p->*_Key) {
                    __swap_with_right_child(__p, __r);
                    __parent    = __r;
                    __l->*_Prev = __r;
                    *__pp       = __r;
                    __pp        = &(__r->*_Right);
                } else {
                    __r->*_Prev = __p;
                    __l->*_Prev = __p;
                    break;
                }
            } else {
                if (__l->*_Key < __p->*_Key) {
                    __swap_with_left_child(__p, __l);
                    __parent = __l;
                    if (__r != nullptr) [[likely]] { __r->*_Prev = __l; }
                    *__pp = __l;
                    __pp  = &(__l->*_Left);
                } else {
                    if (__r != nullptr) [[likely]] { __r->*_Prev = __p; }
                    __l->*_Prev = __p;
                    break;
                }
            }
            __l = __p->*_Left;
            __r = __p->*_Right;
        }
        __p->*_Prev = __parent;
    }

    inline static auto
    __remove_least_leaf(_Node **__pp, const std::size_t __path) noexcept
        -> _Node * {
        std::size_t __mask = __path_bit_mask(__path);
        _Node      *__p    = *__pp;
        while (__mask != 0U) {
            if ((__mask & __path) != 0U) {
                __pp = &(__p->*_Right);
            } else {
                __pp = &(__p->*_Left);
            }
            __p = *__pp;
            __mask >>= 1U;
        }
        *__pp = nullptr;
        return __p;
    }
};
} // namespace __ngr::inline __v0::__core
