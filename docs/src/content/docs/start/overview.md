---
title: Overview
description: What MapLibre Native FFI is, who it serves, and what it excludes.
---

MapLibre Native FFI provides an experimental C API for
[MapLibre Native](https://github.com/maplibre/maplibre-native). It targets
low-level language bindings and host integrations that need a C boundary instead
of direct C++ interop.

The public surface keeps MapLibre Native concepts direct. Runtime, map, style,
camera, resource, event, and render target concepts stay close to the native
library, so bindings can expose predictable behavior across host languages.

Framework concerns such as gestures, widgets, declarative UI, application
lifecycle integration, and full SDK ergonomics belong in downstream adapters.

The C ABI is unstable while the project is pre-1.0.

## Where To Go Next

- Use the [quickstart](/start/quickstart/) to run a supported example.
- Read [concepts](/concepts/) before designing an integration.
- Use the [C reference](/reference/c/) for declarations, ownership rules, and
  status values.
- Read the [development setup](/development/setup/) before contributing.
