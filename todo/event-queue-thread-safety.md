# Event Queue Thread Safety

- severity: P2
- phase: review
- finding: `EventQueue` is mutated from MapLibre observer/frontend callbacks and
  polled from ABI calls without synchronization.
- impact: If callbacks are delivered off the owner thread, `std::deque` and
  `std::string` mutations can race with polling or teardown.
- triage: human
- options: Confirm owner-thread-only callback delivery, or serialize event queue
  access and disconnect/drain callbacks before map destruction.
