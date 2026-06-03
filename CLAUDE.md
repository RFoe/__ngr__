# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`ngr` (`__ngr`) is a single-threaded, C++26 coroutine async runtime built directly on the Linux `io_uring` syscall interface. It is a from-scratch reimplementation of the concurrency primitives found in NVIDIA's [stdexec](https://github.com/NVIDIA/stdexec) (sender/receiver, `io_uring_context`, inplace stop tokens), but with its own coroutine `__task` type and a bump-pointer coroutine-frame allocator instead of senders.

The core is **header-only** under `include/__ngr/`. `source/main.cxx` is the single translation unit that produces the `ngr` executable (currently empty; the previous demo lives in `source/main.cxx.bak`).

## Build, Test, Run

The project **requires Clang + libc++** (it `#include`s libc++ internal headers like `<__coroutine/coroutine_handle.h>` and links `-stdlib=libc++ -lc++ -lc++abi`). GCC/libstdc++ will not compile it. Requires a recent kernel for `io_uring` (IORING_SETUP_DEFER_TASKRUN / SINGLE_ISSUER) and `liburing` via pkg-config.

Three out-of-source build directories already exist, each configured differently:

| Dir          | Generator | Sanitizer            | Use                          |
|--------------|-----------|----------------------|------------------------------|
| `build/`     | Ninja     | UBSan (`-fsanitize=undefined`, Debug default) | primary dev build; has `compile_commands.json` |
| `build-test/`| Makefiles | UBSan                | running ctest                |
| `build-tsan/`| Makefiles | ThreadSanitizer      | data-race checking           |

```bash
# Configure (primary build, Ninja + UBSan). CMakeLists forces Debug.
cmake -G Ninja -B build -DCMAKE_CXX_COMPILER=clang++ .

# Build
cmake --build build

# Run the main executable
./build/ngr

# Run the test suite (ctest target is TEST_ngr; FAIL_ON_STDOUT is set, so any
# stdout from a test counts as failure)
ctest --test-dir build --output-on-failure
# or a single test by name:
ctest --test-dir build -R TEST_ngr --output-on-failure

# ThreadSanitizer build
cmake --build build-tsan && ./build-tsan/ngr
```

`build/compile_commands.json` is what `.clangd` indexes — keep the `build/` dir configured for editor/LSP support.

### How CMake wires targets (CMakeLists.txt)

Sources are glob'd (`CONFIGURE_DEPENDS`) from `source/`, `test/`, and `benchmark/`. There are three executables, all built only if their dir contains sources:
- `ngr` — from `source/*`.
- `test_ngr` — from `test/*` **plus** all of `source/*` except `main.cxx` (regex-excluded), registered as ctest `TEST_ngr`.
- `benchmark_ngr` — same pattern for `benchmark/*`, registered as `BENCHMARK_ngr`.

Warnings are errors everywhere (`-Wall -Wextra -pedantic -Werror`). Release adds `-O3 -march=native`. NAPI busy-poll and an eventfd are registered on the ring at startup.

## Code conventions (important — the whole codebase follows these)

This code deliberately mimics libc++/stdexec internal style. Match it exactly when editing:

- **Reserved-identifier naming throughout**: types/files `__snake_case`, member functions `_M_foo`, static/free helpers `__foo`, enum constants `_S_foo`, enum types `_Sg`, template params `_Ty`/`_Uy`. Everything lives in `namespace __ngr::inline __v0::__core` (or `::__protocal`).
- **Include libc++ granular headers**, not the public umbrella headers — e.g. `<__chrono/duration.h>`, `<__atomic/atomic.h>`, `<__new/allocate.h>` — to keep compile times down.
- **GNU attributes via `__gnu__::` spelling**: `[[__gnu__::__always_inline__, __gnu__::__artificial__, __nodiscard__]]`.
- **`clang-tidy` is suppressed inline** with `// NOLINTBEGIN/END(...)` blocks at file scope; preserve them.
- Formatting is governed by `.clang-format` (4-space indent, custom alignment). Run `clang-format` before committing.

## Architecture

The runtime has three layers. Read them in this order to understand the whole:

### 1. Coroutine task + frame allocator (`__task.hpp`, `__allocator.hpp`, `__generic_task.hpp`)
`__task<T>` is the awaitable coroutine type. Its promise (`__promise`) multiply-inherits four mixins:
- `__promise_storage<T>` — tagged union holding result *or* `exception_ptr` (specialized for `void`).
- `__promise_new_delete` — overrides `operator new/delete` to allocate coroutine frames from a **thread-local bump allocator** (`__allocator`, a chunked stack with `_M_push/_M_pop_stack_frame`). The top-level frame owns the `__allocator` (stashed after the frame via `__alloc_address`); nested frames reuse `__tl_allocator`. This makes a whole coroutine call-tree's frames a single LIFO arena.
- `__promise_saved_tl_alloc` — saves/restores `__tl_allocator` across resumes (see `__generic_task::_M_execute`).
- `__promise_base` — holds the `__continuation_` handle and a `__stop_source`; `__final_awaiter` destroys forwarded stop state and symmetric-transfers to the continuation. `_M_on_await_suspend` chains parent→child continuation and forwards stop tokens.

`__generic_task` is the type-erased base placed on the scheduler's intrusive queues; `__io_uring_task` and `__timer_task` extend it.

### 2. Stop tokens (`__stop_inplace.hpp`)
A lock-free inplace stop facility (`__stop_source` / `__stop_token` / `__stop_callback`), reimplementing stdexec's `inplace_stop_token`. `__state_` packs a stop-requested bit and a spin-lock bit; callbacks form an intrusive doubly-linked list. `__forward_stop_request` chains a parent token's stop into a child source — this is how stop propagates down a coroutine tree.

### 3. io_uring scheduler / event loop (`__scheduler.hpp`)
`__scheduler` *is* the event loop and owns the mmap'd ring.
- `__scheduler_base` does `io_uring_setup` (raw syscalls, not liburing's helpers) with `DEFER_TASKRUN | SINGLE_ISSUER | FEAT_SINGLE_MMAP`, mmaps the SQ/CQ/SQE regions, and registers an eventfd + NAPI busy-poll.
- `__submission_queue::_M_submit` / `__completion_queue::_M_complete` drive SQEs/CQEs. Each `__io_uring_task` carries a 2-function "vtable" (`__manage<__submit, __complete>`) selected at compile time via NTTP wrappers (`__wrapper`/`__nttp`) — there is no runtime vtable pointer per op kind.
- Cross-thread submission: producers push onto `__atomic_task_queue` (`__io_remote_`/`__dr_remote_`, sharded by `__atomic_segment_array`); the loop drains them via `_M_accept_remote_request`. `_M_notify` writes the eventfd to wake a sleeping loop. `__n_submit_in_flight_ == __no_new_submit_ (-1)` is the "shutting down, reject new work" sentinel.
- Timers live in an intrusive min-heap (`__timer_heap` over `__timer_task`, keyed by deadline+sequence). The loop computes the nearest deadline and passes it to `io_uring_enter` via `IORING_ENTER_EXT_ARG` for a timed wait.
- `_M_run_until_stop` is the main loop: complete CQEs → submit pending → if nothing in flight, break; else `_M_io_uring_enter` (sleep) → accept remote → repeat. On stop it cancels everything and drains.

### Data-structure primitives (`__core/`)
`__intrusive_queue` (singly-linked, `_Next` member pointer as NTTP), `__atomic_intrusive_queue` (Treiber-style lock-free push / pop-all-reversed), `__intrusive_heap` (intrusive min-heap), `__atomic_segment_array` (sharded growable array), `__aligned_storage`, `__memory_mapped_region`, `__file_descriptor` (RAII fd). These are the building blocks the scheduler is assembled from.

### Protocol (`__protocal/__message.hpp`)
`__ngr::__protocal` defines wire structs (`__descriptor`, `__proposal`, `__synchronize`, `__operation`) for what appears to be a leased/epoch-based replication protocol. Not yet wired into the runtime.

## Reference material — do not treat as project source

- `include/*.hpp.in` / `*.cpp.in` — **verbatim copies of stdexec / libc++** (Apache-licensed, NVIDIA/Maikel Nadolski headers) kept as design references for the io_uring context, thread pool, timed scheduler, and `memory_resource`. They reference `stdexec/...` paths that don't exist here and are **not compiled**. Use them to see the upstream design `__scheduler.hpp` is modeled on; do not edit them as if they were ngr code.
- `*.bak` files (`source/main.cxx.bak`, `__core/__io_uring_ops.hpp.bak`) — prior iterations kept for reference.
- `__io_uring__.txt` — author's working notes (in Chinese) on io_uring optimizations and correctness pitfalls (ref-counting CQEs before close, `-ECANCELED`/`-ENOBUFS` handling, zero-copy send, buffer-ring selection). Useful context for what the scheduler is evolving toward.

`test/`, `benchmark/`, and `doc/` currently contain only `.gitkeep`.
