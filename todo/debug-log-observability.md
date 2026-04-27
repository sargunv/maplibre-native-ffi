# Debug Log Observability

- severity: P3
- phase: review
- finding: The ABI exposes `MLN_LOG_SEVERITY_DEBUG` as callback-deliverable, but
  MapLibre's `Log::record` skips observers for debug severity.
- impact: Consumers enabling debug logs through the callback API will not
  receive them.
- triage: human
- options: Document debug as platform-log-only/not observable, or avoid exposing
  debug as callback-deliverable until supported.
