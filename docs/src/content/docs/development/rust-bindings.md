---
title: Rust Bindings
description: Contributor notes for Rust bindings.
---

This planned contributor note covers Rust binding design, implementation, tests,
and reference generation.

Tracking issue:
[Add Rust bindings and smoke example](https://github.com/sargunv/maplibre-native-ffi/issues/41).

Planned coverage:

- package and module layout;
- mapping C handles to Rust types;
- ownership and lifetime conventions;
- status, error, and diagnostic mapping;
- event draining and callback safety;
- generated Rust reference documentation;
- Rust tests and examples.
