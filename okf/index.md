---
okf_version: "0.1"
---

# Void Core — Knowledge Bundle (dev)

An [Open Knowledge Format](/references/okf-spec.md) bundle describing **Void Core**
itself: a host-agnostic engine that other applications build on top of. This is the
**dev** bundle — it includes `status:planned` concepts (future work). A shipped
*library* bundle is the same content filtered to `status:current AND audience:library`.

Honesty convention: every concept is tagged `status:current` (built & in the code),
`status:planned` (designed, not built), or `status:deprecated`. A concept may not
claim `current` without a `resource:` link to the code that backs it.

# Planned work

* [Roadmap](/roadmap.md) - index of planned concepts + the near-term build order (dev-only)

# Design — the rationale (dev)

* [Design index](/design/index.md) - design rationale and research tracks (the absorbed `notes/`); start at the [railguard](/design/what-voidcore-is-not.md)

# This app

* [App manifest](/app.md) - Void Core's structured self-description (identity + representation)

# Concepts — the vocabulary

* [Rune](/concepts/rune.md) - the atomic editable unit; a monoid
* [Mantle](/concepts/mantle.md) - runes over a domain; a graph / rewrite system
* [Holiday](/concepts/holiday.md) - the interface to an external system you don't own
* [Glyph](/concepts/glyph.md) - a rune's editability type
* [Tag system](/concepts/tag-system.md) - addressing-by-meaning; the filter grammar
* [Dispatcher](/concepts/dispatcher.md) - the one command entry point
* [Voidscript](/concepts/voidscript.md) - the scripting language over the dispatcher
* [Domain](/concepts/domain.md) - the target a mantle renders/deploys onto
* [UI / UX](/concepts/ui-ux.md) - every app has a user; the core renders nothing but an app must describe its UI/UX — planned
* [Logging & debug](/concepts/logging-debug.md) - the logging spine + debug/testing surface
* [Links](/concepts/links.md) - loose connections (the passive substrate); cross-entity links planned
* [Graph analytics](/concepts/graph-analytics.md) - centrality/clustering tools for agents (a compute holiday)
* [Interaction nets](/concepts/interaction-nets.md) - the mathematical foundation; its executor is Reduce
* [Reduce](/concepts/reduce.md) / [Temper](/concepts/temper.md) / [Scry](/concepts/scry.md) - the three transformation layers (dispatcher verbs at the seam)

# Components — the implementations

* [C core](/components/c-core.md) - the libvoidcore.dll engine
* [Python binding](/components/python-binding.md) - the ctypes binding
* [MeshDB holiday](/components/meshdb-holiday.md) - a local graph BaaS — verified
* [OKF engine](/components/okf-engine.md) - this format, as a holiday (consume/produce/validate v0.1)

# References

* [SPEC.md](/references/spec.md) - the normative contract; section → concept map
* [Glossary](/references/voidcore-glossary.md) - OKF ⇄ Void Core dictionary
* [OKF spec](/references/okf-spec.md) - the format this bundle conforms to

# Sources — external work we lean on

One page per external work, cited by ordinary body link so `linked from` names the blast
radius. All `confidence:asserted` (recalled, not yet checked) — see [sources/](/sources/index.md).

* [Lafont — interaction nets](/sources/lafont-interaction-nets.md) - the foundation of Reduce
* [Ousterhout — A Philosophy of Software Design](/sources/ousterhout-philosophy-of-software-design.md) - where "deep module" comes from
* [WCAG — contrast](/sources/wcag-contrast.md) - the numbers `theme.py` implements
* [Google Cloud — OKF](/sources/google-open-knowledge-format.md) - the format itself
* [Pocock — Skills For Real Engineers](/sources/pocock-skills-for-real-engineers.md) - the adapted skills (MIT)
