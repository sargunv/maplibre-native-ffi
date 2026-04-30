# Contributing

The project is still moving quickly. Bug reports, design feedback, platform
notes, and small fixes are welcome. Large changes are unlikely to be reviewed
promptly unless they were already coordinated with the maintainer.

For discussion, use `#maplibre` on the
[OSM-US Slack](https://slack.openstreetmap.us/).

If you are using AI assistance, review
[MapLibre's AI Policy](https://github.com/maplibre/maplibre/blob/main/AI_POLICY.md).
Verify generated content before requesting review and disclose AI usage in the
PR.

## Repository Layout

- `include/` — public C ABI; keep this C-shaped and FFI-friendly.
- `src/` — C++ implementation around MapLibre Native.
- `tests/` — automated tests in Zig through the C ABI.
- `examples/` — small consumers that exercise the C ABI or language bindings.

See [`docs/development.md`](docs/development.md) for the project boundary and
conventions expected of code changes.

## Development Setup

The recommended workflow is to install [mise](https://mise.jdx.dev/), then run:

```bash
mise install
```

On macOS, additionally install a recent version of Xcode.

Mise installs the pinned project tools and sets up the Git hooks. CMake fetches
MapLibre Native into `third_party/maplibre-native` during configuration. To use
a separate MapLibre Native checkout, set `MLN_SOURCE_DIR` before configuring.

You can also set up your tooling manually without mise -- read
[`mise.toml`](mise.toml) and [`pixi.toml`](./pixi.toml) to see what needs to be
present in your environment.

## Key Commands

- `mise run build` — configure and build the C library.
- `mise run test` — build and run tests.
- `mise run fix` — run formatters and linters.
- `mise run //examples/<project>:run` — run an example application.
