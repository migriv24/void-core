---
type: Design
title: The open application — environment, instantiation, surfaces, and reuse
description: Plan for five ideas (2026-07-04) — a local/host OKF, an app-instantiation standard, a universal sandbox surface, the developer-seat vs user-seat distinction, and cross-app engine reuse — with build phases and open questions for Miguel.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-04T00:00:00Z
---

> Five ideas from Miguel (2026-07-04), thought through against what already exists.
> The through-line is one principle: **Void Core applications are not self-contained.**
> An app describes itself (the [app manifest](/concepts/app-manifest.md)), describes its
> knowledge ([OKF](/design/okf-design.md)), and — this document — describes its
> *environment*, its *birth*, its *surfaces*, and its *reusable parts*, so that agents
> in one app can reach into another. Everything below follows the two disciplines that
> already govern the project: **all I/O is a [holiday](/concepts/holiday.md)** (the core
> stays pure), and **self-description is data readable without running the app**
> ("consuming beats producing").

# 1. The local OKF — a host/environment bundle

**The idea.** An agent driving an app on a Raspberry Pi, on a mesh network, on Android,
or on some entirely weird OS needs to *know that*. Device facts — OS/filesystem
family, architecture, GPU/compute capability, memory, network situation, power class —
are relevant context, and today every agent re-derives them by poking at the shell.

**The shape.** This is not a new format — it is **an OKF bundle whose subject is the
device** instead of the application. Concretely:

- A **probe holiday** (`holidays/host/`) — stdlib-only probing (platform, os, shutil,
  /proc, sysctl, wmic/PowerShell fallbacks…) that *produces* the bundle. Producing an
  environment description is I/O, so it lives at a holiday, never in the core.
- Output is a small bundle (or a single reserved page, `host.md`, `type: Host` —
  the same move as `app.md`/`type: Manifest`) of **probed facts as tags + prose**:
  `os:linux`, `fs:posix`, `arch:arm64`, `gpu:none`, `net:mesh`, `power:battery`,
  `class:sbc`. Known namespaces get normative meaning; everything else lands in the
  `free` axis — the tag system's "known namespaces + free catch-all" shape is exactly
  how a *weird* OS stays describable without a schema change.
- **Facts vs knowledge.** The local bundle is *machine-produced, regenerable cache* —
  per-device, never committed (the `.claude/settings.local.json` of knowledge). It
  carries a probe timestamp and a `confidence:` stamp; the OKF engine's
  validate/refresh job extends naturally to "re-probe if stale."
- **Consumption.** An agent reads it like any bundle (`okf query "net:mesh"`); an app
  adapts to it (Hormiga's degraded-console-on-missing-DLL problem is precisely a
  host-bundle lookup today done by hand).

**Abstraction test:** the page must be useful for a machine we never imagined. Bare
minimum guaranteed fields ≈ what Python's stdlib can always answer (os family, path
separator, arch); everything past that is optional tags. Fallback chain like the
manifest reader: full probe → partial probe → "unknown host" stub.

# 2. The instantiation standard — how an app is born

**The idea.** If agents build Void Core apps, the "must-dos" of starting one can't
live in Miguel's head or scattered across four project histories. There are now four
real apps (Fountain, Portfolio Manager, Hormiga, FaultSack) — enough precedent to
distill a standard instead of inventing one.

**The shape.** A reserved OKF reference page — the **application standard** — that is
a *lintable checklist*, in three tiers:

- **MUST** (an app isn't a Void Core app without these): an `app.md` manifest; an OKF
  bundle skeleton (`index.md`, `log.md` as changelog, honesty tags); a pinned core
  version (vendored or release-pinned — the Hormiga pattern is the reference); all
  I/O behind holidays / the effect handler; state as the §2 state document; a UI/UX
  declaration (even if the answer is "none — CLI only", see §3 below).
- **SHOULD**: conformance to the SPEC subsets it claims; transform rules authored as
  data (`config.transform`) where used; a roadmap; the secret/junk hygiene sweep
  (learned the hard way — Hormiga burned its history over one hardcoded secret).
- **MAY**: representation tier, surface holiday (§3), portable mantles (§5).

**Enforcement path:** checklist page first (data, agents can follow it today);
`holidays/okf/validate.py` grows an `--app` mode that lints another app's bundle
against the MUST tier second; a `voidcore init` scaffold third (only if the checklist
proves insufficient — a scaffold that drifts from the standard is worse than none).

# 3. The visual OKF / the sandbox surface

**The idea.** FaultSack wants an "application sandbox" tab: run the studied app inside
the study tool. To do that *universally* — not as a FaultSack hack — an app must be
able to declare: does it have a UI at all? and where are the seams (holidays) where an
external tool can render, drive, and observe it?

**The shape — a capability ladder, not a widget standard.** The core still renders
nothing. What we standardize is the *contract* an app can expose, in levels, each one
a declared entry in the manifest + a holiday behind it:

- **L0 — dispatcher (universal, already true).** Every app is drivable through
  `dispatch()` and observable through `export_state`/`ls`/`describe`. A CLI-only app
  is *already sandboxable*: the sandbox is a Void Console (Hormiga ships one today).
  FaultSack's sandbox tab gets every Void Core app at L0 for free.
- **L1 — headless render.** A **surface holiday**: `render(ctx) -> artifact`
  (HTML string / PNG / text frame). The app's manifest declares
  `ui.kind: web|cli|native|device|none` and `ui.surface: <holiday name>`. The sandbox
  shows the artifact; state changes come from L0 dispatches. This is the SPEC §3.3
  `render` **[ext]** seam, promoted from a glyph footnote to an app-level contract.
- **L2 — interactive surface.** The surface holiday adds `drive(event)` and
  `snapshot() -> {artifact, state-hash}` — the sandbox forwards clicks/keys, the app
  maps them to dispatches. This is where a *visual* app becomes testable by an agent
  ("click this, assert that") without screen-scraping.
- **L3 — full embed.** The host process embeds the app's actual UI (iframe, child
  window). Out of core scope; the manifest just declares how (`ui.embed: iframe|…`).

**Declaration** goes in the manifest's representation tier (flat keys, existing
parser): `ui.kind`, `ui.surface`, `ui.embed`, plus per-entry-point capability tags.
"Does this app even have a UI" becomes a machine-readable fact — which also finally
gives the [UI/UX concept](/concepts/ui-ux.md) its concrete "how do you declare it"
answer.

**Division of labor:** Void Core specs the ladder + manifest keys + surface-holiday
ops; FaultSack (or any consumer) owns its sandbox UI/controls. Novel consumers are the
point: a launcher that live-previews apps, Hormiga rendering another app's mantle in a
newsletter, an agent A/B-testing two apps' surfaces.

# 4. Two seats — developer agents vs user agents

**The idea (4a).** An agent *developing* Hormiga (editing its code, reading its dev
OKF, running its tests) and an agent *using* Hormiga (driving the Void Console inside
the shipped app, on behalf of an end user who brings their own LLM) are different
audiences with different surfaces, permissions, and knowledge needs.

**The shape.** Name them: the **builder's seat** and the **operator's seat**. Most of
the machinery already exists and just needs the distinction drawn through it:

| | builder's seat | operator's seat |
|---|---|---|
| knowledge | dev OKF bundle (`audience:dev`) | library bundle (`audience:library`) + manifest |
| surface | repo, SPEC, tests, full dispatcher | the app's shipped console / UI; the dispatcher as the app exposes it |
| state | may rebuild/migrate it | owns their data; undo is their safety rail |
| core version | chooses/bumps the pin | receives it vendored |

The `audience:` axis was built for exactly this split — extend it from *pages* to the
whole contract: which verbs, which holidays, which bundle each seat gets. Whether the
operator seat is *enforced* (a restricted dispatcher profile — no `deploy`, no
`config set`, no `script set`?) or *conventional* (the app curates what its console
exposes) is an open question below. Today every app hand-rolls this choice; a named
concept page plus a recommended default profile would stop the drift.

**(4b) feeds §5:** the *builder's* seat is also the seat that pulls proven engines out
of *other* apps — which needs the reuse protocol:

# 5. Engines as nodes — cross-app reuse and portable mantles

**The idea.** An application is a bunch of modular engines cooperating; wrap an engine
in a holiday and another app can reuse it. And beyond code: one app should be able to
take a *mantle* from another app and adopt it. Radical openness — agents in two apps
trading working parts.

**The shape — two protocols, one existing precedent.**

- **Engine reuse (code).** The pattern already happened twice without being named:
  `holidays/graph/` (graph analytics) and `holidays/okf/` (knowledge) are engines
  wrapped in holidays, built in Void Core, consumed by FaultSack. The standard is:
  an **engine ships as (a) code, (b) an OKF page describing it (`type: Engine` —
  already in use), and (c) a holiday wrapper whose `describe()` states capabilities**.
  The planned tagged **holiday registry** is the discovery layer: "give me a
  `kind:knowledge` holiday" instead of an import path. Reuse = vendor the engine +
  read its bundle; no new mechanism, just the standard written down and the registry
  built.
- **Portable mantles (data + behavior).** A mantle is *already* nearly portable — it
  is a slice of the state document, and its behavior rides with it as data
  (`config.transform`: temper rules, selectors, reducers). What's missing is the
  envelope. Define a **mantle capsule**: `{ mantle, glyph defs it needs,
  config.transform slice, required-holiday manifest (names + capabilities),
  provenance (source app id/version, snapshot hash — `provenance()` exists) }`.
  Export via a verb (`mantle export <name>` — or `dump`'s canonical form grows this);
  import validates the envelope: glyphs it can register, holidays it can satisfy,
  and a declared policy for what it can't (refuse / import-degraded with opaque
  runes / prompt). Cross-entity links (rune↔mantle↔holiday, SPEC §3.7's planned
  extension) is what later lets an imported mantle keep pointing home.

# 6. FaultSack notes (for its agent, not built here)

- **Codebase-as-node-graph (idea 5) is already three-quarters real.** FaultSack's
  locked decision #2 *is* a unified node graph (concepts + files/symbols; OKF
  cross-links + imports + NLP edges), and centrality already runs over it. Rendering
  it as a graph instead of (only) a folder tree is a front-end choice over data it
  already has. The Void-Core-flavored move: **materialize the snapshot as a mantle**
  — runes = files/symbols, `link` edges = imports/references, tags = language/
  cluster/centrality-band. Then the module map is just a mantle view, the graph
  analytics holiday reads it natively, developer notes can attach as facets on the
  file-runes, and the same mantle is exportable as a capsule (§5) for the building
  agent.
- **Sandbox tab:** target L0 first (a Void Console over the studied app's dispatcher
  — works for every app on day one), L1 when the surface holiday exists. Don't build
  a bespoke driver; consume the ladder.

# 7. Build plan

Standards before machinery; every phase leaves the system consistent.

- **Phase A — write the contracts (cheap, high leverage).** This doc merged with
  Miguel's answers → then: `host bundle` concept page + `host.md`/`type: Host`
  reserved shape; **application standard** reference page (the 3-tier checklist);
  **two seats** concept page; manifest gains spec'd optional `ui.*` keys (doc only);
  capability-ladder section added to [ui-ux](/concepts/ui-ux.md); engine-reuse
  standard + capsule envelope spec'd in [holiday](/concepts/holiday.md)/SPEC §10.
  Roadmap + glossary updated. No code.
- **Phase B — probe holiday.** `holidays/host/` producing the local bundle, cached +
  refreshable; `validate` learns the staleness rule. Hormiga's installer becomes the
  first consumer (replaces its hand-rolled platform checks).
- **Phase C — sandbox surface L0/L1.** Spec the surface-holiday ops
  (`render`/`snapshot` as effect ops); a reference implementation over one existing
  app (Portfolio Manager is the simplest visual one); FaultSack sandbox tab consumes
  L0, then L1.
- **Phase D — capsules + registry.** `mantle export`/import with the envelope +
  policy; the tagged holiday registry (already on the roadmap) as the discovery
  layer; `validate --app` (the MUST-tier lint) lands here once the standard has
  survived contact with a second author.

Sequencing rationale: A unblocks every downstream agent immediately (they build
*against* the contracts); B is small and pays Hormiga back; C is FaultSack-driven so
it has a real consumer from day one; D is where openness becomes mechanical, and it
deliberately comes last — envelopes and registries designed before two real apps have
traded anything would be speculation.

# 8. Questions for Miguel

1. **Local bundle location + lifecycle.** Per-device (`~/.voidcore/host/`, shared by
   all apps on the machine) or per-app-instance (`<app>/okf.local/`)? Probe at app
   start, on demand, or on a staleness clock? (My lean: per-device, probe-on-demand
   with a cached copy + timestamp.)
2. **Privacy line for the host bundle.** Hostname, username, precise hardware, and
   network topology are fingerprinting-grade data, and user-seat agents may be
   third-party LLMs. What's in the default bundle vs behind an opt-in tag
   (`sensitivity:` axis?), and should apps be able to ship a redaction policy?
3. **How prescriptive is the instantiation standard?** Checklist only, lint
   (`validate --app`) that FaultSack/CI can run, or full scaffold tooling? And is a
   UI/UX declaration truly a MUST for v1 of the standard, or a SHOULD?
4. **Sandbox v1 scope.** Is L1 (headless render → static artifact, drive via
   dispatcher) enough for FaultSack's first sandbox tab, or do you want L2
   (event `drive()`) in the first cut? L2 roughly doubles the contract surface.
5. **Capsule import policy + trust.** When app B lacks a glyph/holiday a capsule
   needs: refuse, import-degraded (opaque runes, inert rules), or interactive
   resolution? And since apps are "super open" — is provenance (source id + snapshot
   hash) enough, or do you want any notion of signing/trust before agents start
   auto-importing each other's mantles?
6. **Operator seat: enforced or conventional?** Should the core offer a restricted
   dispatcher profile (deny-list of dev verbs for user-seat agents), or is it each
   app's job to curate its exposed console? (Enforced = safer default for BYO-LLM
   users; conventional = zero core surface area.)
7. **Mesh case: self-description only, or discovery too?** Does `net:mesh` in the
   host bundle imply a later "find *other* devices' bundles/apps on the mesh"
   capability (that's a discovery protocol — significant), or is describing *this*
   node enough for now?
8. **Vocabulary check before it fossilizes:** "host bundle" (local OKF), "surface
   holiday" + the L0–L3 ladder, "builder's/operator's seat", "mantle capsule",
   "engine" (already a type). Any renames while they're still cheap?

# Status

`planned` — direction document; no contract or code yet. Phase A items land as
concept/reference pages after the §8 questions are answered; contracts then graduate
to `SPEC.md` per the normal design→spec flow.
