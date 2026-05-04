# AGENTS.md

## Workflow

Use `mise run test` to build and test. Use `mise run fix` to run formatters and
linters. Run examples only when useful, with a brief timeout as most are GUI
apps, not one-shot tests.

Feature changes need tests through the C ABI when practical.

Campsite rules apply: leave anything you touch tidier than when you found it.

## Conventions

- Read
  [`docs/src/content/docs/development/conventions.md`](docs/src/content/docs/development/conventions.md)
  before before working on the C API.
- Inspect `third_party/maplibre-native` or `MLN_SOURCE_DIR` before inferring
  native behavior.
