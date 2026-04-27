# Log Callback Threading

- severity: P1
- phase: review
- finding: `mln_log_callback` is invoked directly from MapLibre logging paths,
  including async logging worker threads.
- impact: This conflicts with the architecture goal of avoiding direct
  host-language callbacks from arbitrary native threads.
- triage: human
- options: Queue/poll log events instead, or remove direct callbacks from the
  ABI.
