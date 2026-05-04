---
title: Go Bindings
description: Contributor notes for Go bindings.
---

This planned contributor note covers Go binding design, implementation, tests,
and reference generation.

Tracking issue:
[Add Go bindings](https://github.com/sargunv/maplibre-native-ffi/issues/43).

Planned coverage:

- package and module layout;
- mapping C handles to Go types;
- ownership and lifetime conventions;
- status, error, and diagnostic mapping;
- event draining and callback safety;
- generated Go reference documentation;
- Go tests and examples.
