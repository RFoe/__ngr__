#pragma once

#include <__bit/bit_ceil.h>
#include <__cstddef/size_t.h>
#include <cassert>

namespace __ngr::inline __v0::__core {
[[__gnu__::__always_inline__, __gnu__::__artificial__]]
inline auto _S_path_bit_mask(const std::size_t __n) noexcept -> std::size_t {
    return std::bit_ceil(__n + 1) >> 2U;
}

template <auto _Key, auto _Prev, auto _Left, auto _Right>
struct __intrusive_heap;

template <
    typename _Node, typename _KeyT, _KeyT _Node::*_Key, _Node *_Node::*_Prev,
    _Node *_Node::*_Left, _Node *_Node::*_Right>
struct __intrusive_heap<_Key, _Prev, _Left, _Right> {
    void _M_push_front(_Node *__node) noexcept {
        __size_++;
        _Node **__pcur   = &__root_;
        _Node  *__cur    = __root_;
        _Node  *__parent = nullptr;

        std::size_t __path = __size_;
        std::size_t __mask = _S_path_bit_mask(__size_);
        while (__mask != 0U) {
            if (__node->*_Key < __cur->*_Key) {
                __node->*_Prev = __parent;
                __cur->*_Prev  = __node;
                *__pcur        = __node;
                do {
                    if ((__mask & __path) != 0U) {
                        __node->*_Right       = __cur;
                        __node->*_Left        = __cur->*_Left;
                        __cur->*_Left->*_Prev = __node;
                        __node                = __cur;
                        __cur                 = __cur->*_Right;
                    } else {
                        __node->*_Right = __cur->*_Right;
                        __node->*_Left  = __cur;
                        if (__cur->*_Right != nullptr) [[likely]] {
                            __cur->*_Right->*_Prev = __node;
                        }
                        __node = __cur;
                        __cur  = __cur->*_Left;
                    }
                    __mask >>= 1U;
                } while (__mask != 0U);

                assert(__node->*_Right == nullptr);
                __node->*_Left = nullptr;
                return;
            }
            if ((__mask & __path) != 0U) {
                __pcur = &(__cur->*_Right);
            } else {
                __pcur = &(__cur->*_Left);
            }
            __parent = __cur;
            __cur    = *__pcur;
            __mask >>= 1U;
        }

        *__pcur         = __node;
        __node->*_Prev  = __parent;
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
        _Node *__cur   = _S_remove_last_leaf(&__root_, __size_);
        _Node *__top   = __root_;
        __cur->*_Right = __top->*_Right;
        __cur->*_Left  = __top->*_Left;
        __root_        = __cur;

        __size_--;
        _S_shift_down(__root_, &__root_, nullptr);
        __root_->*_Prev = nullptr;
    }

    auto _M_remove(_Node *__node) noexcept -> bool {
        if (__node->*_Prev == nullptr && __node != __root_) { return false; }
        assert(__root_ != nullptr);
        assert(__size_ != 0);
        _Node *__cur = _S_remove_last_leaf(&__root_, __size_);
        __size_--;
        if (__cur == __node) [[unlikely]] {
            __node->*_Prev = nullptr;
            return true;
        }
        __cur->*_Right = __node->*_Right;
        __cur->*_Left  = __node->*_Left;
        _Node **__pcur __attribute__((__uninitialized__)); // NOLINT
        _Node  *__parent = __node->*_Prev;
        if (__parent != nullptr) {
            if (__parent->*_Right == __node) {
                __pcur = &(__parent->*_Right);
            } else {
                __pcur = &(__parent->*_Left);
            }
            *__pcur = __cur;
            if (__cur->*_Key < __parent->*_Key) {
                _S_shift_up(__cur, __parent);
                __node->*_Prev = nullptr; // Make node erasure-aware.
                return true;
            }
        } else {
            __pcur  = &__root_;
            *__pcur = __cur;
        }
        _S_shift_down(__cur, __pcur, __parent);
        (*__pcur)->*_Prev = __parent;
        __node->*_Prev    = nullptr;
        return true;
    }

    _Node      *__root_ = nullptr;
    std::size_t __size_ = 0;

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static void
    _S_swap_with_right_child(_Node *__parent, _Node *__child) noexcept {
        __parent->*_Right = __child->*_Right;
        __child->*_Right  = __parent;

        _Node *__tmp     = __parent->*_Left;
        __parent->*_Left = __child->*_Left;
        __child->*_Left  = __tmp;
    }

    [[__gnu__::__always_inline__, __gnu__::__artificial__]]
    inline static void
    _S_swap_with_left_child(_Node *__parent, _Node *__child) noexcept {
        _Node *__tmp      = __parent->*_Right;
        __parent->*_Right = __child->*_Right;
        __child->*_Right  = __tmp;

        __parent->*_Left = __child->*_Left;
        __child->*_Left  = __parent;
    }

    inline static void _S_shift_up(_Node *__cur, _Node *__parent) noexcept {
        _Node  *__sentinel = nullptr;
        _Node **__parent_x =
            (__cur->*_Left != nullptr) ? &(__cur->*_Left->*_Prev) : &__sentinel;
        _Node **__parent_y = (__cur->*_Right != nullptr)
                               ? &(__cur->*_Right->*_Prev)
                               : &__sentinel;

        *__parent_y           = __parent;
        _Node *__grand_parent = __parent->*_Prev;
        do {
            *__parent_x = __parent;
            if (__parent->*_Right == __cur) {
                __parent_x = &(__parent->*_Left->*_Prev);
                _S_swap_with_right_child(__parent, __cur);
            } else {
                __parent_x = &(__parent->*_Right->*_Prev);
                _S_swap_with_left_child(__parent, __cur);
            }
            __parent_y = &(__parent->*_Prev);
            if (__grand_parent->*_Right == __parent) {
                __grand_parent->*_Right = __cur;
            } else {
                __grand_parent->*_Left = __cur;
            }
            __parent       = __grand_parent;
            __grand_parent = __parent->*_Prev;
        } while (__cur->*_Key < __parent->*_Key);
        *__parent_x   = __cur;
        *__parent_y   = __cur;
        __cur->*_Prev = __parent;
    }

    inline static void
    _S_shift_down(_Node *__cur, _Node **__pcur, _Node *__parent) noexcept {
        _Node *__right = __cur->*_Right;
        _Node *__left  = __cur->*_Left;
        while (__left != nullptr) {
            if ((__right != nullptr) && (__right->*_Key < __left->*_Key)) {
                if (__right->*_Key < __cur->*_Key) {
                    _S_swap_with_right_child(__cur, __right);
                    __parent       = __right;
                    __left->*_Prev = __right;
                    *__pcur        = __right;
                    __pcur         = &(__right->*_Right);
                } else {
                    __right->*_Prev = __cur;
                    __left->*_Prev  = __cur;
                    break;
                }
            } else {
                if (__left->*_Key < __cur->*_Key) {
                    _S_swap_with_left_child(__cur, __left);
                    __parent = __left;
                    if (__right != nullptr) [[likely]] {
                        __right->*_Prev = __left;
                    }
                    *__pcur = __left;
                    __pcur  = &(__left->*_Left);
                } else {
                    if (__right != nullptr) [[likely]] {
                        __right->*_Prev = __cur;
                    }
                    __left->*_Prev = __cur;
                    break;
                }
            }
            __left  = __cur->*_Left;
            __right = __cur->*_Right;
        }
        __cur->*_Prev = __parent;
    }

    inline static auto
    _S_remove_last_leaf(_Node **__pcur, const std::size_t __path) noexcept
        -> _Node * {
        std::size_t __mask = _S_path_bit_mask(__path);
        _Node      *__cur  = *__pcur;
        while (__mask != 0U) {
            if ((__mask & __path) != 0U) {
                __pcur = &(__cur->*_Right);
            } else {
                __pcur = &(__cur->*_Left);
            }
            __cur = *__pcur;
            __mask >>= 1U;
        }
        *__pcur = nullptr;
        return __cur;
    }
};
} // namespace __ngr::inline __v0::__core
