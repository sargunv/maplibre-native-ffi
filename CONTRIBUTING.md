# Contributing

This repository accepts changes that improve the C ABI, low-level language
bindings, tests, examples, documentation, and platform evidence around MapLibre
Native. It is still moving quickly, so small focused fixes, bug reports, design
feedback, and platform notes are easiest to review.

Coordinate large API, ABI, ownership, threading, async, render target, or
binding changes before opening a pull request.

Use `#maplibre` on the [OSM-US Slack](https://slack.openstreetmap.us/) for
discussion.

## Development Setup

Install [mise](https://mise.jdx.dev/), then install the pinned project tools:

```bash
mise install
```

On macOS, install a recent version of Xcode.

Mise installs the tools used by the repository and sets up Git hooks. CMake
fetches MapLibre Native into `third_party/maplibre-native` during configuration.
Set `MLN_SOURCE_DIR` before configuring to use a separate MapLibre Native
checkout.

For manual setup, use [`mise.toml`](mise.toml) and [`pixi.toml`](pixi.toml) as
the list of required tools.

## Making Changes

Read [`docs/development.md`](docs/development.md) before changing behavior or
public interfaces. It owns the project scope and the conventions for ABI shape,
errors, ownership, threading, callbacks, render targets, tests, and examples.

Keep pull requests focused on one reviewable change. The reviewer should be able
to connect the use case, public behavior, implementation, and validation without
separating unrelated work.

## Validation

Build and test with:

```bash
mise run test
```

Use targeted example runs when they provide evidence that automated tests
cannot, such as rendering or host integration:

```bash
mise run //examples/<project>:run
```

The repository also provides:

- `mise run build` to configure and build the C library;
- `mise run fix` to run formatters and linters.

Pre-commit runs the configured formatters and linters on changed files.

## Pull Requests

Open a pull request when the change is ready for review and include:

- the problem or use case;
- the public API or behavior change, if any;
- the validation you ran;
- platform limitations or native MapLibre behavior you checked;
- follow-up work that should remain separate.

If you use AI assistance, review
[MapLibre's AI Policy](https://github.com/maplibre/maplibre/blob/main/AI_POLICY.md),
verify generated content before requesting review, and disclose AI usage in the
pull request.
