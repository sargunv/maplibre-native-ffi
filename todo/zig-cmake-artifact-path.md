# Zig CMake Artifact Path

- severity: P3
- phase: review
- finding: `build.zig` hardcodes `build` as the library and rpath directory,
  while CMake can use arbitrary or multi-config build directories.
- impact: `zig build test` and `zig build run` can load stale or missing
  libraries.
- triage: human
- options: Add a Zig option for the CMake build directory, or have Zig
  drive/query the CMake artifact path.
