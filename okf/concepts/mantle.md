---
type: Concept
title: Mantle
description: A group of runes over a domain, plus the relationship graph and rules between them.
resource: core/src/model/mantle.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-06-18T00:00:00Z
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

# What is built vs planned

- **Current**: the data shape, rune CRUD with name-reference repointing, the `layout`
  graph and `rules` are *stored and inspected*.
- **Planned**: the rule **reducer** that actually *executes* rewrites — deliberately
  deferred (see [interaction nets](/concepts/interaction-nets.md)). Rules are modeled
  now, reduced later.

# Status

`current` (structure). The rule executor is `planned`. See `SPEC.md` §3.4.
