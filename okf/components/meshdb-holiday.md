---
type: Holiday
title: MeshDB holiday
description: A local, offline graph Backend-as-a-Service holiday backed by MeshDB (Bolt/Cypher); the default data backend for graph-shaped apps.
resource: holidays/meshdb/meshdb_holiday.py
tags: [status:current, audience:dev, confidence:verified]
timestamp: 2026-07-01T00:00:00Z
---

The first concrete [holiday](/concepts/holiday.md): a local, offline graph
**Backend-as-a-Service** backed by [MeshDB](https://github.com/mesh-db/meshdb) (a
real Rust Cypher graph DB) in single-node mode. Well suited as a default data backend
for graph-shaped apps — deliberately *not* SQLite, because in a graph DB a
[rune](/concepts/rune.md)'s [tags](/concepts/tag-system.md) are edges, so tag
membership becomes a native graph query.

# Graph model

```
(:Rune   {id, name, glyph, content, f_who, f_what, f_when, f_where, f_why, f_how})
(:Mantle {name})
(:Tag    {name})
(r:Rune)-[:IN_MANTLE]->(m:Mantle)
(r:Rune)-[:TAGGED]->(t:Tag)
```

A rune is identified globally by `id` (`spirit.id`, frozen) and named uniquely within a
mantle; `content` is opaque, so it crosses as a JSON string and the six facets land as
`f_who`..`f_how`. Per SPEC §5 a rune's own `name` and `glyph:<name>` count as tags, so the
filter translator tests those alongside real `:Tag` edges. This is the model the weighted
tag-graph (SPEC §5) can grow into.

# Key decision

The holiday is a **Bolt/Cypher client + lifecycle manager, not a linked dependency**.
All Rust lives in the `meshdb-server` child process; the holiday spawns/attaches and
talks `bolt://`. So MeshDB does *not* force a Rust rewrite of the
[C core](/components/c-core.md).

# Built

`MeshDBHoliday.local_baas()` (spawns single-node server, no auth, own data dir) +
the holiday interface (insert/get/query/update/delete/describe) over runes; a
[tag-filter → Cypher](/concepts/tag-system.md) translator (unit-tested); a smoke test
asserting `holiday.query(expr) == core ls --tag expr`.

# Status

`current` (the holiday itself, **verified 2026-06-18**). `meshdb-server` built and the
end-to-end smoke passed: 3 runes synced core → MeshDB, full CRUD + describe, and
**tag-query parity 7/7** — every tag expression returns the same rune set from
MeshDB/Cypher as from the core's own `ls --tag`. The effect-handler binding this was
waiting on now exists (`VoidCore.set_effect_handler`, 2026-06-28), so routing the
[dispatcher](/concepts/dispatcher.md)'s `save` through this holiday is unblocked; wiring
a real app through it end-to-end is the remaining step. See the bundle [log](/log.md).
