# AGENTS.md

## Source Of Truth

- `DESIGN.md` explains the intended architecture.
- `ROADMAP.md` explains implementation order and acceptance criteria.
- If implementation disproves the design, update `DESIGN.md` in the same change.
- If sequencing or acceptance criteria change, update `ROADMAP.md` in the same
  change.

## Progress Tracking

- Each milestone should have clear acceptance evidence before being considered
  done. Mark milestones completed in the roadmap.
- Record important discoveries in `DESIGN.md` if they affect architecture.
- Record sequencing changes, deferred work, or newly split milestones in
  `ROADMAP.md`.
- Do not leave important architectural decisions only in commit messages or
  chat.

## Implementation Notes

- Keep `include/maplibre_native_abi.h` as the public product boundary.
- Keep implementation-only helpers out of the public header.
- Treat local MapLibre Native behavior as evidence; inspect
  `third_party/maplibre-native` or `MLN_SOURCE_DIR` before guessing.

## Validation

- Every milestone should add or update a smoke test, example, or automated test
  that demonstrates its acceptance criteria.
- Prefer tests/examples that consume the C ABI rather than C++ internals.
- If a milestone cannot be fully validated yet, document the remaining risk in
  `ROADMAP.md`.
