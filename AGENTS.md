# AGENTS.md

## Workflow

Use `mise run test` to build and test. Use `mise run fix` to run formatters and
linters. Run examples only when useful, with a brief timeout because most are
GUI apps, not one-shot tests.

Feature changes need tests through the C ABI when practical.

Campsite rules apply: leave anything you touch tidier than when you found it.

## Documentation

When working on documentation, determine up front who the audience is and
whether the documentation is a Tutorial, Guide, Reference, or Explanation,
according to the
[Diátaxis Framework](https://raw.githubusercontent.com/evildmp/diataxis-documentation-framework/refs/heads/main/start-here.rst).
State your determination for the user, then load and follow the appropriate
guide before making changes:

- [Reference](https://raw.githubusercontent.com/evildmp/diataxis-documentation-framework/refs/heads/main/reference.rst)
  usually covers comments attached to source code (e.g., Doxygen).
- [Guides](https://raw.githubusercontent.com/evildmp/diataxis-documentation-framework/refs/heads/main/how-to-guides.rst)
  usually covers user-facing documentation.
- [Explanation](https://raw.githubusercontent.com/evildmp/diataxis-documentation-framework/refs/heads/main/explanation.rst)
  usually covers contributor-facing or user-facing documentation.
- [Tutorials](https://raw.githubusercontent.com/evildmp/diataxis-documentation-framework/refs/heads/main/tutorials.rst)

Before finishing documentation changes, apply the prose rules from
[Writing Clearly and Concisely](https://raw.githubusercontent.com/obra/the-elements-of-style/refs/heads/main/skills/writing-clearly-and-concisely/SKILL.md):
use active voice, positive statements, concrete language, parallel structure,
and no needless words.
