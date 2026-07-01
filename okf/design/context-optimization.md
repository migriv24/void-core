---
type: Design
title: Context-size optimization
description: Context-length optimization as a core pillar: a summarization scaffold buildable now, RL-ready later.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-01T00:00:00Z
---

# Context-Size Optimization as a Core Pillar (and the RL / Vector North Star)

> Source: SUPO — *"Scaling LLM Multi-turn RL with End-to-end Summarization-based
> Context Management"* (arXiv 2510.06727).
>
> Framing: context optimization is **not** merely an LLM nicety. It is part of
> "giving a language to how software structures itself," and it is also plain good
> CLI UX for a *human*. A Void-Core CLI should never flood whoever is driving it —
> human or agent — with more than they asked for.

---

## 1. First line of defense: altitude (already in the model)

Most context blowups are avoidable *by design*, before any summarization:

- **Read verbs return summaries, not dumps.** `describe` gives a glyph summary +
  facets; it does not print every field. (`SPEC.md §7`.)
- **Tag-expression targeting (`@expr`) and query-backed mantles.** An agent
  operates on *selections*, never the full set. (`SPEC.md §5, §10.2`.) This is the
  same principle that lets Hormiga's agent build a newsletter without listing the
  whole database.
- **Chunked listing — a concrete gap to close.** `ls` over a 10k-rune mantle must
  **paginate** (cursor- or page-based: `ls --tag x --page 2 --size 50`), and
  `describe` on a big mantle should return **shape** (counts by tag/axis) rather
  than contents. Add a **context budget** notion to read verbs. This is buildable
  now and belongs in the next SPEC revision.

---

## 2. Second line: the summarization scaffold (SUPO, de-RL'd)

SUPO's deep insight, separated from its training machinery: **the structure of
summarization-based context management is a deterministic protocol you can build
today, with no neural network.** The RL only learns *what* to put in the summary;
the scaffold around it is fixed.

The protocol (adapted from SUPO's rollout algorithm):

```
maintain a working context for a session/REPeL run
on each step:
  if |context| < L:            append (action, observation)   # grow normally
  else if summaries < S:       emit a SUMMARIZE step           # compress
       on summarize:           context := (initial_goal, summary)   # reset
                               record a summary-boundary index
  else:                        stop (budget exhausted)
```

- **Threshold-trigger, compress-and-reset.** When the working context exceeds a
  budget `L`, compress the history into a **task-relevant summary**, then continue
  from `(initial goal + summary)`. Working context stays bounded.
- **Track summary-boundary indices.** Pointers to where each compression happened.
- **Today's summarizer can be heuristic or a single LLM call** (via an LLM
  holiday). No training required. The scaffold is identical whether the summarizer
  is a regex, a prompt, or (later) a learned policy.
- **It scales at "test time."** SUPO shows a model trained with 2 summary rounds
  generalizing to 23 — i.e., the scaffold keeps working far past where it was
  tuned. For us: the protocol is robust even with a dumb summarizer now.

Naming candidate (Void Core likes evocative terms — spirit, rune, mantle,
holiday): call a summary checkpoint a **molt** — the session sheds its bulky
accumulated context and keeps a compact "skin." (Provisional.)

---

## 3. Designing so RL can drop in later (free, do-it-now choices)

We will **not** train networks now. But cheap structural choices keep the door open:

- **Make the logging spine an RL-clean trajectory record.** Every dispatch already
  produces `(state-ish, verb+args, result)`. If the log captures these as tidy
  `(observation, action, outcome)` tuples with a place for a later **reward**, then
  a future RL setup has its trajectories for free. This is just disciplined logging.
- **Identify where RL would live** (so the interfaces are clean):
  1. the **summarizer** — what to keep in a molt (SUPO's exact target);
  2. the **self-healing helper agent** — the fix policy (see
     [dsl-and-pel.md](/design/voidscript-dsl.md), REPeL);
  3. **action/tool selection** — which verb/tool next
     ([tools-memory-extensions.md](/design/agent-tools-memory.md));
  4. **retrieval** — which runes/tags to surface into context.
  All four are *a policy over Void Core's action space (dispatcher verbs) under a
  task reward.* Same shape; build the action space and trace format well now.

---

## 4. The far-future north star (held honestly)

The stated dream: the **entire command set and DSL expressed as high-dimensional
vectors** pointing to abstract action sequences — an embedding layer (or a LoRA)
that lets an LLM *fully* integrate with the CLI, rather than emitting text that we
parse.

- **Why our current choices serve it.** A small, **regular grammar**
  ([dsl-and-pel.md](/design/voidscript-dsl.md)) + a **finite agent/glyph vocabulary with typed
  ports** ([interaction-nets.md](/design/interaction-nets-theory.md)) + **clean action traces**
  (§3) are precisely what maps cleanly into a learned embedding space. A sprawling,
  irregular CLI would be unembeddable. So the careful core *is* the groundwork.
- **Why it's out of scope.** Full vectorization requires *training a model* (likely
  our own, possibly a foundation model) — which the user has rightly ruled out for
  now. We design *toward* it; we do not pretend we can build it yet.
- **The honest middle.** Long before vectors, the regular grammar already gives the
  big practical win PEL describes: **constrained generation** lets today's
  off-the-shelf LLMs emit only valid Void Core programs. That is the reachable
  version of "the LLM integrates with the CLI," available now.
