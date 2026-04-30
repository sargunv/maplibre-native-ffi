# AGENTS.md

## Workflow

- `mise run fix` -- run formatters and linters
  - clang-tidy is slow -- pre-commit hooks will run this for you on changed
    files. You don't need to run it yourself during a task.
- `mise run test` -- build project and run tests
- if you run the example apps, always use a brief timeout

## Implementation Notes

- Treat local MapLibre Native behavior as evidence; inspect
  `third_party/maplibre-native` (fetched on demand by CMake — see
  `cmake/mln_version.cmake`) or `MLN_SOURCE_DIR` before guessing.
- Every feature should add or update a smoke test, example, or automated test
  that demonstrates its acceptance criteria.
- Prefer tests/examples that consume the C ABI rather than C++ internals.
- If work cannot be fully validated yet, document the remaining risk in the
  relevant issue.

## Project Conventions

- Keep `include/maplibre_native_abi.h` as the public product boundary.
- Keep the public ABI C-shaped: plain structs, opaque handles, status returns,
  explicit ownership, and no C++/STL/exceptions/templates across the boundary.
- Keep implementation-only helpers out of the public header.
- Keep framework behavior above the ABI: gestures, declarative state, widget
  lifecycle, coroutine/Flow/Promise APIs, and toolkit-specific adapters belong
  in language/UI bindings, not the core C ABI.
- Preserve the event-driven async model: C ABI calls return status and report
  asynchronous native work through drained events, not futures, promises,
  coroutines, or arbitrary host-language callbacks.
- Treat native callbacks as low-level escape hatches only; document threading,
  reentrancy, lifetime, and whether callbacks may call back into the ABI.
- Keep map state separate from render targets. A map may have zero or one
  attached render target at a time, either a texture session or a native surface
  session.
- The ABI is currently unstable (`mln_abi_version() == 0`); do not add
  backward-compatibility shims for changed structs or functions unless
  explicitly requested.
- Mark every exported `MLN_API` C++ definition `noexcept`; status-returning ABI
  functions must catch exceptions and convert them to `mln_status`.
- Keep diagnostics paths non-throwing where practical; fallback diagnostics are
  better than letting error reporting violate the C ABI boundary.
- Keep public ABI docs complete but concise. Do not duplicate preconditions or
  threading rules in prose when the `Returns:` status bullets already state
  them.
