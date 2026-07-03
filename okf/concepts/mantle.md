---
type: Concept
title: Mantle
description: A group of runes over a domain, plus the relationship graph and rules between them.
resource: core/src/model/mantle.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-07-01T00:00:00Z
---

A **mantle** is a set of [runes](/concepts/rune.md) over a
[domain](/concepts/domain.md), plus a `layout` (a relationship graph of
[links](/concepts/links.md)) and `rules` (behavior). One target can carry several
stacked mantles.

# Mantle as a graph / rewrite system

The mantle is where structure lives: its `layout.edges` form a graph over the runes,
and its `rules` are intended to be [interaction-net](/concepts/interaction-nets.md)
rewrite rules — "a mantle controls the rewrite rules of its runes." The graph is the
passive part; the rewrite system is the active part layered on top.

# What is built

- The data shape, rune CRUD with name-reference repointing, the `layout` graph and
  `rules` are *stored and inspected* in the [C core](/components/c-core.md).
- The rule **reducer** that *executes* rewrites — deliberately deferred at first
  ("model it as a net now, reduce it later") — is now built as
  [Reduce](/concepts/reduce.md): the `reduce` verb builds the active mantle's net from
  `layout.edges` and rewrites it to normal form.

# Status

`current` (structure in the C core; the executor at the [dispatcher](/concepts/dispatcher.md)
seam). See `SPEC.md` §3.4.
