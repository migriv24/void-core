# Project skills

Workflow skills for building Void Core. Adapted from **Matt Pocock's "Skills For Real
Engineers"** (https://github.com/mattpocock/skills, MIT) — fundamentals-first, small and
composable. The only local change: pointers to a `CONTEXT.md` were broadened to also read
this project's **OKF bundle** (`okf/`), which is our knowledge artifact (we keep OKF rather
than a CONTEXT.md — see `okf/references/engineering-vocabulary.md`).

| skill | invoke | what it's for |
|---|---|---|
| **codebase-design** | model or `/codebase-design` | the deep-module vocabulary — small interface, deep implementation, at a clean **seam** (+ `DEEPENING.md`, `DESIGN-IT-TWICE.md`) |
| **tdd** | model or `/tdd` | red-green-refactor in vertical slices; behavior-not-implementation tests (+ `tests.md`, `mocking.md`, `refactoring.md`) |
| **diagnosing-bugs** | model or `/diagnosing-bugs` | build a tight, red-capable feedback loop *first*, then hypothesize |
| **grilling** | model or `/grilling` | relentless one-question-at-a-time alignment on a plan |
| **grill-me** | `/grill-me` | user-invoked entry to a grilling session |
| **handoff** | `/handoff` | compact the conversation into a handoff doc for another agent |

Not installed (deliberately): `grill-with-docs` / `domain-modeling` impose a `CONTEXT.md` +
ADR layout that overlaps our OKF; the issue-tracker/triage/PRD process machinery is heavier
than this project needs; TS-specific and personal skills don't apply.
