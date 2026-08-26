---
type: Concept
title: Links
description: Loose, non-reactive connections between runes (and, later, mantles/holidays) — the passive substrate under layout edges, bindings, and tag references.
resource: core/src/model/mantle.c
tags: [status:current, audience:dev, audience:library, confidence:verified, foundation]
timestamp: 2026-06-18T00:00:00Z
---

A **link** is a loose connection between two entities. It is directed (or not),
optionally labeled with a `relation`, optionally `weight`ed, and **does nothing on its
own**. Links are the *passive substrate*; behavior is a separate layer.

A first-class link primitive now exists in the [dispatcher](/concepts/dispatcher.md):
`link <from> <to> [--relation r] [--weight w] [--undirected]`, `unlink`, and a
read-only `links [<ref>]`, backed by the mantle's `layout.edges` (`rune move` is a
legacy alias). Links **may dangle** — an endpoint need not exist yet (`validate`
reports dangling endpoints, it doesn't forbid them), exactly like an
[OKF](/components/okf-engine.md) markdown link or the memory store's `[[wikilink]]`.

# Why this is a distinct concept

Void Core currently has four half-overlapping relationship mechanisms with no
unifying primitive:

| mechanism | what it is | reactive? |
|---|---|---|
| mantle `layout.edges` | `{from, to, relation}` graph; created by `rune move`, repointed on rename, dropped on remove | no (stored, not executed) |
| rune `relations` | reserved, empty | no |
| cross-mantle `bindings` | `{from:{…on}, to:{…do}}` | **yes** (fires) |
| name-as-tag | `tags:[othername]` references a rune | no |

The missing idea is the substrate under all of them: **a link is just a connection;
a binding/rule is a link that has been given [interaction-net](/concepts/interaction-nets.md)
firing semantics.** Links are the graph; interaction nets are a behavior layer on a
*chosen subset* of links.

# Key properties

- **Non-reactive by default.** Connection without consequence; opt into behavior.
- **May dangle.** A link to a not-yet-existing target is legal, not an error —
  exactly like an [OKF](/components/okf-engine.md) markdown link ("not-yet-written
  knowledge") and the agent memory store's `[[wikilink]]`.
- **Cross-kind.** Runes, mantles, and holidays can all be link endpoints.

# Convergence with OKF

OKF links *are* this primitive: untyped directed edges, meaning carried by prose,
dangling-tolerant. So OKF is not adding links to Void Core — it is a *view* of the
links that already (informally) exist. This is the entry in the
[glossary](/references/voidcore-glossary.md): OKF link ⇄ Void Core link.

# An edge carries three things — reify anything else

An edge is **`relation`, `direction`, and `weight`**, and that is the whole vocabulary.
`weight` is not the first of an open-ended set of attributes; it is the only one. A host
that wants a second — a confidence, a source, an as-of date, an author — must **reify the
edge as a [rune](/concepts/rune.md)** and link to *that*.

This reads like a workaround and is usually the better model. Void Hormiga wanted a
confidence on an *attributed-to* edge (*"this statement is attributed to this person,
0.61"*) and found, on reifying it, that the evidence belonged on the statement all along:
a statement has exactly one speaker, so what was uncertain was the **claim**, not the
edge. Reification asks "what is the thing this attribute is really about?" and the answer
is usually a noun the model was missing. (Reported 2026-08-17; the rule was already true
and simply not written down, so a host reading this page reasonably tried to add a second
attribute.)

# Status

`current` for the **rune↔rune** primitive (verified 2026-06-18): first-class
`link`/`unlink`/`links` verbs over `layout.edges`, with `relation`/`weight`/`directed`
and dangling-tolerance, repoint-on-rename and drop-on-remove (SPEC §3.7). This is what
the [OKF engine](/components/okf-engine.md) maps concept links onto. The reactive
counterpart (a link that fires) is a `binding`; the weighted
[tag graph](/concepts/tag-system.md) is the tag↔tag analogue.

`planned`: extending links **cross-entity** (rune↔[mantle](/concepts/mantle.md)↔[holiday](/concepts/holiday.md))
at host level, and folding `bindings` + the rune `relations` field into the one
primitive so there is a single link store rather than several.
