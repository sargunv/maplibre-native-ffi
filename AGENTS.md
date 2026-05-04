# AGENTS.md

Read
[`docs/src/content/docs/development/conventions.md`](docs/src/content/docs/development/conventions.md)
before changing design, C ABI, ownership, threading, async, errors, tests,
examples, or render targets. Inspect `third_party/maplibre-native` or
`MLN_SOURCE_DIR` before inferring native behavior.

Use `mise run test` to build and test. Skip `mise run fix` unless explicitly
requested; pre-commit runs formatters and linters on changed files. Run examples
only when useful, with a brief timeout.

Feature changes need evidence through the C ABI when practical. Leave touched
code tidier than you found it.
