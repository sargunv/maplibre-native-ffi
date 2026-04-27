# Runtime RunLoop Ownership

- severity: P1
- phase: review
- finding: Each map constructs its own `mbgl::util::RunLoop`, but MapLibre's
  Darwin run loop asserts that no scheduler already exists on the thread and
  sets/clears the thread-local scheduler.
- impact: Multiple maps on one runtime can abort or break the remaining map's
  scheduler.
- triage: human
- options: Move `RunLoop` ownership to `mln_runtime`, or explicitly reject
  multiple live maps per runtime.
