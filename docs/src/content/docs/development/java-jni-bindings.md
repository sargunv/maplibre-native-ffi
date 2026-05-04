---
title: Java (JNI) Bindings
description: Contributor notes for Java (JNI) bindings and Android demo work.
---

This planned contributor note covers Java (JNI) binding design, Android
integration, implementation, tests, and reference generation.

Tracking issue:
[Add Java (JNI) bindings and Android demo](https://github.com/sargunv/maplibre-native-ffi/issues/47).

Planned coverage:

- package and module layout;
- mapping C handles to Java JNI-backed types;
- ownership and lifetime conventions;
- status, error, and diagnostic mapping;
- event draining and callback safety;
- generated Java reference documentation;
- Android tests and examples.
