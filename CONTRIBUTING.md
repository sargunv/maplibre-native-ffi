# Contributing

Use `#maplibre` on the [OSM-US Slack](https://slack.openstreetmap.us/) for
discussion.

## Before Making Changes

See
[`docs/src/content/docs/development/setup.md`](docs/src/content/docs/development/setup.md)
for platform setup, pinned tools, and local commands.

Read
[`docs/src/content/docs/development/conventions.md`](docs/src/content/docs/development/conventions.md)
before changing behavior or public interfaces. It owns the project scope and the
conventions for ABI shape, errors, ownership, threading, callbacks, render
targets, tests, and examples.

Coordinate large API, ABI, ownership, threading, async, render target, or
binding changes before opening a pull request.

Keep pull requests focused on one reviewable change. The reviewer should be able
to connect the use case, public behavior, implementation, and validation without
separating unrelated work.

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
