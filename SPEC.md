# Void Core — Specification (v0.2)

> **The contract.** This document defines Void Core independently of any one
> implementation, so that multiple implementations (the [C core](core/README.md), the
> JS prototype/oracle in [`src/`](src/), and host-embedded Python impls) can be checked
> for conformance against the *same* behavior.
>
> - `ARCHITECTURE.md` is the JS prototype's (legacy) design narrative.
> - The design rationale ("why") lives in the OKF: [`okf/design/`](okf/design/index.md).
> - **This file is the normative contract (the "what").** When this file and the
>   others disagree about behavior, this file wins; fold corrections back here.
>
> Status keys used below: **[impl]** = built and shipped in the reference impl;
> **[ext]** = specified extension for an embedding host, *not* in the reference impl yet
> (see §10); **[seam]** = part of the dispatcher *contract* but implemented once at a
> seam (the Python `voidcore.Dispatcher`) rather than reimplemented per binding — any
> binding MAY provide it, with the Python module as the reference impl. Conformance is
> defined per-status in §11.

---

## 1. What Void Core is

Void Core is a host-agnostic engine with five parts:

1. A **data model** — `spirit`, `rune`, `glyph`, `mantle`, `domain`, `binding`.
2. A **command dispatcher** — one entry point that the CLI, a GUI, and the script
   runner all call. Every mutation is undoable and dirty-tracked.
3. A **tag system** — axis-typed tags with a filter-expression language.
4. A **script runner** — Voidscript, a small terminal-complete language over the
   dispatcher.
5. A **logging spine** + an **adapter seam** for syncing the abstract model to a
   real backend (files, a database, an output artifact).

A concrete application = Void Core + registered glyphs + adapters + domain(s).
Example shapes: a website manager (rewrites site source files), a newsletter builder
(persists to a DB / renders output), a game-content forge. Applications are developed
in their own repos; Void Core stays isolated. The **[ext]** sections below reference an
embedding host as a concrete extension driver.

**Host-language-agnostic (a defining principle).** Void Core is an *overlay*, not
a language or a runtime. The application it sits over may be written in **any**
language (HTML/JS, Python, C++, Rust, …); only the **adapter** (§9) must speak the
host's language. Void Core structures and orchestrates an application — it does
**not** compile, replace, or *execute* it. (Contrast Bend/NeLA, which are languages
with their own reduction runtimes; see `okf/design/what-voidcore-is-not.md`.)

An implementation of Void Core itself MAY be written in any language. It MUST
preserve the data shapes (§3), the dispatcher result contract (§6), tag semantics
(§5), and the verb semantics (§7) marked **[impl]**.

---

## 2. The state document  **[impl]**

All persistent working state is one serializable document:

```jsonc
{
  "version": 1,
  "domains": { "<name>": <Domain> },   // §3.5, keyed by domain name
  "mantles": [ <Mantle>, ... ],        // §3.4
  "bindings": [ <Binding>, ... ],      // §3.6 (host-level, cross-mantle)
  "scripts": { "<name>": "<voidscript source>" },
  "config":  { "<key>": <scalar> },
  "active":  { "mantle": "<name|null>", "domain": "<name|null>" },
  "_baseline": [ <Mantle>, ... ]       // snapshot at last Save Progress (dirty-tracking)
}
```

- Loading a malformed/absent document MUST yield the empty state above.
- `_baseline` is the comparison point for `status`/`diff` and the restore point
  for `revert`. It is replaced on every `save`/`deploy`.
- The state document is the *abstract* working state. Writing edits into a real
  backend is the adapter's job (§9), not the store's.

---

## 3. Data model

### 3.1 Spirit — a rune's identity  **[impl]**
```jsonc
{ "id": "rune_9fa3c1b7e2", "name": "nervous-bubble" }
```
- `id` (the *real ID*): minted once as `"<prefix>_<random hex>"`, frozen forever,
  never reused. Implementations MUST mint a collision-resistant random id.
- `name` (the *human handle*): editable, MUST be unique within its mantle. It
  doubles as a tag (§5), so renaming MUST repoint references (§3.4).

### 3.2 Rune — the atomic editable unit  **[impl]**
```jsonc
{
  "spirit":  <Spirit>,                 // §3.1
  "glyph":   "text",                   // editability type; MUST be a registered glyph name
  "facets":  { "who":"", "what":"", "when":"", "where":"", "why":"", "how":"" },
  "tags":    ["science", "outcome:concepts"],
  "content": { /* glyph-specific; core does NOT interpret it */ },
  "placement": null,                   // optional explicit position (reserved)
  "relations": []                      // optional; reserved
}
```
- The six **facets** are always present (may be empty). They are uniform textual
  metadata so any rune can be described/reasoned about by a human or LLM.
- `content` is opaque to the core; only the rune's **glyph** (§3.3) interprets it.
- Hydrating a partial/legacy rune MUST fill missing fields with the defaults above
  and MUST reject a rune with no `spirit`.

### 3.3 Glyph — the editability registry  **[impl]**
A glyph binds a rune type to how it is edited and how it is described.
```jsonc
{
  "glyph":   "text",
  "label":   "Text block",
  "editor":  "text",                   // which GUI editor / CLI prompt
  "fields":  ["value"],                // content field names
  "schema":  null,                     // optional
  "describe":   "(rune) -> short textual summary of content",   // function
  "newContent": "() -> default content payload for a fresh rune" // function
}
```
- Built-in glyphs an implementation MUST provide: `text`, `richtext`, `image`,
  `imageList`, `color`, `link`, `group`.
- Applications register additional glyphs (Biology: `bubble`, `dialogueLine`,
  `characterConfig`; Hormiga: newsletter block types).
- **[ext]** A glyph MAY also define `render(rune, ctx) -> string` for applications
  whose runes produce an output artifact (e.g. a newsletter block → HTML) rather
  than writing back into source files.

### 3.4 Mantle — runes over a domain + their rules  **[impl]**
```jsonc
{
  "id":     "mantle_b21f...",
  "name":   "biology-hub",             // required, unique in state.mantles
  "domain": "biology-portfolio",       // domain name, or null
  "runes":  [ <Rune>, ... ],
  "tags":   {},                        // tag definitions (reserved)
  "layout": { "edges": [ <Link> ] },   // the link graph (§3.7)
  "rules":  [ ]                        // event/behavior rules (reserved)
}
```
Required operations (semantics fixed):
- `findRune(mantle, ref)` — match by `spirit.name` OR `spirit.id`; null if none.
- `addRune` — reject a duplicate `spirit.name`.
- `removeRune` — also drop any `layout.edges` referencing the rune.
- `renameRune` — keep `spirit.id`; reject a taken name; repoint every reference
  to the old name in other runes' `tags` and in `layout.edges`.
- `rules` are **persisted from day one**; the executor that consumes a mantle's
  rewrite rules is the `reduce` verb at the seam (§7), driven by
  `config.transform.reduce` — the inline `rules` array itself remains reserved (§12).

### 3.5 Domain — the base a mantle renders/deploys onto  **[impl]**
```jsonc
{ "name":"...", "repo":null, "liveUrl":null,
  "build":null, "deploy":null, "preview":null, "port":null }
```
Each field except `name` is an optional shell command or value. The domain is the
seam between the abstract model and the real world.
- **[ext]** A domain need not be a website target. For Hormiga a domain is an
  *output/distribution target* (HTML/email export + send). Implementations SHOULD
  treat the website fields as one specialization and allow application-defined
  ones (resolved by the adapter, §9).

### 3.6 Binding — a cross-mantle connection  **[impl]**
Lives at host level because it references more than one mantle.
```jsonc
{
  "id":   "bind_...",
  "name": null,
  "from": { "mantle":"content", "rune":"intro", "on":"reach" },
  "to":   { "mantle":"click",   "rune":"line-1", "do":"fire" },
  "note": ""
}
```
A ref string is `"mantle:rune"` or `"rune"` (default mantle supplied by caller).
The reaction engine that *fires* bindings is reserved (priorities first, full
Petri-net semantics later — see `okf/design/interaction-nets-theory.md`).

### 3.7 Link — a loose connection in the layout graph  **[impl]**
```jsonc
{ "from":"intro", "to":"methods", "relation":"supports",
  "weight": 1.0, "directed": true }
```
A **link** is a passive (non-reactive) connection between two runes, stored in the
mantle's `layout.edges`. `relation` is a free label (may be `""`), `weight` a number
(default `1.0`), `directed` a bool (default `true`). Semantics:
- Links are the **passive substrate**: storing one does nothing on its own. (A
  reactive connection that *fires* is a `binding`, §3.6 — a link with behavior.)
- A link **MAY dangle**: an endpoint need not exist (it may be not-yet-created
  knowledge). `validate` reports dangling endpoints; it does not forbid them.
- Created/updated via `link` (and the `rune move` alias); removed via `unlink`;
  listed via `links` (§7). Repointed on rename, dropped on remove (§3.4).
- A rune's `relations` field (§3.2) is reserved/superseded by the mantle link graph.
- Cross-entity links (rune↔mantle↔holiday) are a planned extension; v1 links are
  rune↔rune within one mantle.

---

## 4. Identity & reference rules (normative)
- A rune is referenced by `spirit.name` **or** `spirit.id` anywhere a `<ref>` is
  accepted.
- A `<ref>` beginning with `@` is a **tag expression** selecting *many* runes
  (§5). Mutating verbs that accept `<ref>` MUST apply to every selected rune.
- `spirit.id` is immutable; `spirit.name` is mutable but reference-repointing
  (§3.4).

---

## 5. Tag system  **[impl]**

- A rune is matched by any of its `tags` **or** its own `spirit.name`. (A rune's
  name is implicitly a tag — this is how runes reference each other.)
- A tag is a string, conventionally `namespace:value` (e.g. `group:science`,
  `month:june`, `status:draft`).
- **Fundamental axes** — the fixed upper ontology every tag classifies into:
  `where`, `what`, `who`, `when`, `state`, `free`. A namespace maps to one axis
  (e.g. `site`/`group`/`section`/`outcome` → `where`; `trigger` → `when`;
  `status` → `state`); unknown namespaces → `free`. This is the interlingua that
  lets two independent tag sets merge by typed union (see
  `okf/concepts/tag-system.md`).
- **Filter-expression grammar** (used by `ls --tag`, `@…` targets, `foreach`):
  ```
  expr   := or
  or     := and ( ("OR" | "||") and )*
  and    := not ( ("AND" | "&&") not )*
  not    := ("NOT" | "!") not | atom
  atom   := "(" or ")" | TAG
  ```
  `TAG` matches if it is in the rune's tag set (including its name). Operators are
  case-insensitive. An empty expression matches all runes.
- **One evaluator, exposed over the FFI.** The C core exports the grammar as
  `vc_tag_match(expr, tags_json) -> 1|0|-1` (`tags_json` = a JSON array of tag
  strings; include the entity's name for name-as-tag matching; stateless,
  thread-safe; bound in Python as `VoidCore.tag_match(expr, tags)`). Hosts that
  filter *external* entities by tag expression — e.g. holiday rows behind
  `effect query "<expr>"` (§10) — MUST evaluate through this seam rather than
  reimplement the grammar, so query-over-holiday means exactly what `ls --tag`
  means.

---

## 6. Dispatcher contract  **[impl]**

One dispatcher backs the CLI, GUI, and script runner.

**Result shape** — every verb returns:
```jsonc
{ "ok": <bool>, "lines": [<string>, ...], "data": <any|null> }
```
- `lines` is human-readable output. `data` is the machine value (what `--json`
  surfaces and what `foreach`/`$(…)` consume). `ok=false` signals failure without
  throwing across the boundary.
- An unknown verb returns `ok:false` with a hint, and logs a warning.
- A handler that throws is caught: the dispatcher logs `ERROR <verb>` and returns
  `ok:false` with the message in `lines`.

**Argument & flag parsing** (shared by CLI and Voidscript):
- argv is split respecting single/double quotes.
- `--flag` is boolean `true`; `--flag=value` or `--flag value` (for known
  value-flags: `tag`, `level`, `tail`, `message`/`m`, `state`, `port`, `as`,
  `name`, `note`, `mantle`) takes a value; `-x` is a short boolean (or value-flag).
- everything else is positional.

**Mutation invariants:** every mutating verb pushes an undo frame (snapshot of
mantles+active) *before* mutating; the redo stack is cleared on new mutation;
the undo stack is bounded (reference impl: 200).

**Threading contract:** a dispatcher instance (a `VC_Manager` in the C core) is
**not thread-safe** — it holds one mutable state document and unsynchronized
undo/redo stacks. Hosts MUST serialize all calls that take the same instance
(`vc_dispatch`, `vc_export_state`, `vc_register_glyph`, …) behind a lock, or
confine the instance to one thread. (Hormiga's per-manager Python lock is the
intended pattern, not an over-caution.) Distinct instances are independent and
may run concurrently. Host callbacks (§9 log sink / effect handler) are invoked
synchronously on the dispatching thread, *inside* the dispatch — a callback MUST
NOT re-enter the same instance. Stateless library functions (`vc_tag_match`,
`vc_alloc_str`, `vc_free_str`, `vc_version`) are safe from any thread.

---

## 7. Verb catalog (semantics)  **[impl unless marked]**

### 7.1 POSIX surface (aliases)

Aliases are **argument-aware desugarings** applied to the argv *before* routing —
one semantics, many spellings; an alias MUST NOT fork behavior (the alias and its
canonical form are indistinguishable downstream, including in undo labels and
`is-mutating` classification). The mental model for terminal-trained hands and
agents: **mantle ≈ directory, rune ≈ file, tag expression ≈ glob**.

| alias | desugars to |
|---|---|
| `cd <mantle>` | `use <mantle>` |
| `cd` / `cd /` | `use` (deactivate — see cold-start semantics below) |
| `pwd` | `where` |
| `rm <ref>` | `rune rm <ref>` |
| `mv <a> <b>` | `rune rename <a> <b>` |
| `cp <a> [<b>]` | `rune dup <a> [<b>]` |
| `mkdir <name>` | `mantle new <name>` |
| `grep <q>` | `find <q>` |
| `man [verb]` | `help [verb]` |
| `?` | `help` |
| `quit` | `exit` |
| `dump` | `export` |

(This replaces the earlier bare-rename alias `rm`→`rune`, under which `rm x` meant
`rune x` — a usage error. `rm x` now means `rune rm x`, which is what a POSIX hand
expects.)

**Cold-start semantics** (so a fresh session is self-explanatory):
- `ls` with **no active mantle** lists the mantles (root-`ls`) instead of erroring;
  `data` = array of mantle names. With an active mantle it lists runes as below.
- `use` with no argument (or `use /`) **deactivates** the current mantle — sets
  `active.mantle` to `null`, returning to the mantle list. Not undo-tracked (like
  `use <mantle>`).

### 7.2 Verbs

**Read (no mutation):**
| verb | meaning |
|---|---|
| `describe [<ref>]` | whole-mantle summary, or one rune's glyph summary + 6 facets + its bindings |
| `ls [--tag <expr>]` | list runes (optionally tag-filtered); `data` = array of names. No active mantle → root-`ls` (§7.1): lists mantles instead. The `--tag` expression ends at the next `--flag` token, so a trailing flag (e.g. `--json` appended by a `$(…)` capture) never joins into the expression |
| `tree` | mantle → runes → layout edges, indented |
| `get <ref> [<field>]` | a content field value, or all content |
| `find <query>` | substring search over name/content/facets/tags |
| `cat <ref>` | raw rune JSON |
| `status [--dirty]` | change set vs `_baseline`; `--dirty` → `ok` reflects dirtiness (for scripts) |
| `diff [<ref>]` | saved-vs-working diff |
| `history [--tail N]` | undo-stack labels |
| `glyphs` | registered glyphs |
| `axes [all]` | this mantle's tags bucketed by fundamental axis (`all` = list the axes) |
| `mantles` | all mantles (active marked) |
| `domain` | the active domain's fields |
| `where` | active mantle + domain (pwd-like) |
| `validate [--quiet]` | check duplicate names, unregistered glyphs, dangling layout edges |
| `links [<ref>]` | list links (§3.7), optionally only those touching `<ref>`; `data` = link objects |

**Mutate (each undoable):**
| verb | meaning |
|---|---|
| `set <ref> <field> <value>` | set a content field on every targeted rune |
| `setjson <ref> <field> <json-value>` | like `set`, but the value is parsed as JSON (number/bool/array/object/string; invalid JSON falls back to a plain string) — how a host/UI (and the transformation verbs below) write typed or structured content |
| `facet <ref> <who\|what\|when\|where\|why\|how> <value>` | set one facet |
| `tag <ref> +add -remove …` | add/remove tags |
| `rune new <glyph> <name>` | mint a rune (auto spirit.id, glyph's `newContent()`) |
| `rune rm\|rename\|dup\|move …` | remove / rename / duplicate / set a link (`move` = `link` alias) |
| `link <from> <to> [--relation r] [--weight w] [--undirected]` | create/update a link (§3.7); endpoints MAY dangle |
| `unlink <from> <to> [--relation r]` | remove matching link(s) |
| `mantle new <name>` | create a mantle over the active domain (becomes active) |
| `bind <from> <on> <to> <do> [--name]` | create a cross-mantle binding (validates refs) |
| `bindings [<rune>] [--mantle m]` / `unbind <id\|name>` | list / remove bindings |
| `undo [N]` / `redo [N]` | walk the undo/redo stacks |
| `batch '<json-array>'` | apply a JSON array of command strings **atomically** (rollback on any failure) as **one undo frame**. The payload is inline — the core does no file I/O (§9) |

**Lifecycle:**
| verb | meaning |
|---|---|
| `save` | Save Progress: run the `save` adapter (write real backend), snapshot `_baseline`, persist state |
| `deploy [--message m]` | Update Website: save adapter, then `git add/commit/push` (if repo) + run `domain.deploy`; streamed & logged |
| `build` | run `domain.build` only |
| `preview start\|stop\|status` | run/kill `domain.preview` as a child process |
| `effect <op> [args...]` | invoke the host effect handler with a custom op (the holiday boundary beyond save/deploy/build/preview); returns the handler's parsed result as `data`. Read-only to the core (not undo-tracked) |
| `revert` | discard working changes back to `_baseline` |

**Scripts / system:**
`script run|ls|show|new|set` · `log [--tail N] [--level L]` · `use <mantle|domain>`
· `config [get|set …]` · `export [<file>]` · `import <file>` · `version` · `help [<verb>]`
· `exit`.

Reserved namespace: `agent …` (the LLM seam — not wired).

**Transformation verbs** (the three layers — `okf/design/transform-layers.md`) **[seam]**:

These are the dispatcher surface of Void Core's three pure transformation layers. They
**coexist with** the verbs above (they don't replace any) and return the same
`{ok, lines, data}` contract. They are pure (no I/O / clock / RNG); the mutating ones write
their result back through the verbs above (`setjson` / `tag`), so they stay undoable.

| verb | layer | meaning |
|---|---|---|
| `scry [<tagexpr>] [--select <name>] [--limit N] [--locale/-audience/-role/-date V]` | Scry | **read**, no mutation: project a view over the active mantle. Bare form filters by tag-expression → `data` = matching names; `--select` runs a registered named projection (its own where/sort/limit) under an optional `Context` → `data` = projected views |
| `temper [<ref>]` | Temper | **mutate**: normalize one rune (or all in the active mantle) to canonical form via the registered, idempotent Temper pass; writes back only what changed. `data` = changed refs |
| `materialize <ref> <field>=<value> …` | Scry | **mutate**: freeze resolved values into a rune's content (the explicit, undoable "bake"). `data` = changed refs |
| `reduce [--into <name>] [--commit]` | Reduce | **derive**: build the active mantle's interaction net (port indices ride each edge's `relation` as `"i:j"`), rewrite it to normal form with the registered/loaded reducer, return the derived mantle in `data` (source untouched — pure + previewable); `--commit` also installs it as a live mantle |

**Implementation status.** The layers are implemented **once** (the tested Python modules
`scry/`, `temper/`, `reduce/`) and exposed via a dispatcher **seam** —
`voidcore.Dispatcher`, a drop-in **superset** of `VoidCore.dispatch` that handles the
transform verbs and delegates every other command to the core unchanged. This is the
**[seam]** marking: the verbs are part of the dispatcher *contract* (any binding may
implement them), with Python as the reference impl — the same discipline as the tag
evaluator (one C impl + a conformance-tested Python twin). `scry` / `temper` /
`materialize` / `reduce` are all built at the seam (the `reduce` executor is `reduce/`; its
reducer + port signatures are authored as data, `voidcore.spec.reducer_from_spec`). The seam
also offers an opt-in **temper-on-write** mode
(`Dispatcher.temper_on_write()`): the registered Temper pass runs automatically after every
mutating verb, re-normalizing the rune(s) it targeted — so canonical-form invariants hold
even for **raw** dispatcher edits, not only an app's high-level methods.

Temper passes, named Selectors, and the Reduce ruleset may be **authored as data**
(`voidcore.spec`: `temper_from_spec` / `selector_from_spec` / `reducer_from_spec`) rather
than code, and stored in the state document under `config.transform`
(`{"temper": [...], "selectors": {name: {...}}, "reduce": {"signatures": {...}, "rules": [...]}}`).
The seam loads them with `Dispatcher.load_from_config()`, so the rules persist and reload
with the data they govern. A multi-write transform pass (`temper` / `materialize`, a
temper-on-write repair, a `reduce --commit`) is applied as **one atomic undo frame** via the
`batch` verb — one author-facing action, one undo. (The tokenizer's per-token buffer grows
dynamically, so a large `batch` payload is never truncated.)

---

## 8. Voidscript

A terminal-complete language interpreted over the dispatcher. Every non-control
line is a dispatcher command.

**Core subset [impl]** — required for reference conformance; implemented in both
the C core and the JS oracle:

- **Comments** `# …`; statement separators newline or `;`; blocks `{ }`.
- **Variables** `let x = <expr>`; interpolation `$x` / `${x}`.
- **Command capture** `let d = $(status --json)` — `$( … )` runs a command;
  with `--json` it captures `data`, else stdout text.
- **Conditionals** `if <cond> { } elif <cond> { } else { }`. A condition is either
  an expression (when it contains an operator) or a command (truthy ⇔ `ok`).
- **Operators** `== != < > <= >= && || !`, parentheses, numeric/string coercion.
- **Loops** `while <cond> { }` (guarded), `repeat <n> { }`,
  `foreach v in (<command>) { }` (iterates the command's `data` array, else lines);
  `break` / `continue`.
- **Errors** `assert <cond>` (halt 1 if false).
- **Flow** `halt [code]`, `return [value]`, `echo`/`print`.
- **Args** `script run <name> a b c` exposes `$1 $2 $3` and `$@`.
- **Result** `{ ok, lines, data }` like any command; `halt N` → `ok = (N==0)`.

**Advanced constructs (oracle only; planned [impl])** — implemented in the JS
oracle, `planned` for the C core, and *not* required for reference conformance:

- **Functions** `def name(params) { }` ; call `name(args)`. An unknown call name
  falls back to running a saved script of that name.
- **Errors** `try { } catch (e) { }`, `on error stop|continue` (default stop).
- **Flow** `wait <ms>`, `include "file"`, `call <script> args`.

---

## 9. Adapter seam & logging  **[impl]**

- **Adapters** are supplied by the application via the **effect handler**
  (`vc_set_effect_handler`; bound in the Python binding as `VoidCore.set_effect_handler(fn)`),
  `fn(op, args) -> dict|str|None`. `save(ctx)` writes the abstract model into the real
  backend (Biology: rewrite site source files; Hormiga: persist to the DB / render output);
  `save`/`deploy`/`build`/`preview` invoke it. The generic **`effect <op> [args...]`** verb
  routes *any* host op through the same seam and returns its result as `data` — so read
  effects like `query(tagExpr) -> [rune]` are reachable (Hormiga: `effect query "<expr>"`).
  Host return strings are built with `vc_alloc_str` so the core can free them across the FFI
  boundary without a cross-allocator hazard.
  **[ext]** A `render(ctx)`/per-glyph `render` seam produces output artifacts for
  applications whose runes are not file-backed.
- **Logging** is one spine shared by CLI/GUI/scripts. Format
  `[ISO-timestamp] LEVEL op: message`, level ∈ `INFO|WARN|ERROR`. Long operations
  (deploy/build/preview) MUST stream line-by-line, persist to a log file, and be
  retrievable via `log`. The copied log is the unit handed to an agent for repair.
  - **Hormiga note:** Hormiga already owns a logger, an undo/redo command stack,
    and a tag store. A conforming Hormiga implementation MUST bind the dispatcher
    onto those existing facilities rather than run a second copy (see
    `Hormiga/VOIDCORE_INTEGRATION.md` §3.2).

---

## 10. Extensions for Hormiga  **[ext]** (not in reference impl)

These are specified here so implementations converge, but they are **not** part of
the JS reference impl and are **not** required for reference conformance.

### 10.1 Holiday — reaching a domain you don't control
A **holiday** is the protocol interface to an external system the application does
*not* own (an API, a cloud DB, a file host). **It is the same object as a Hormiga
Antfarm node**; the Antfarm protocol types ARE the holiday interface:
```
query(tagExpr, opts)  -> [<Rune>|<row>]      // resolve a tag expression remotely
get(ref)              -> <payload>
insert(rune|row)      -> <ref>
describe()            -> { capabilities, kind, status }
```
- Holidays are registered in a **holiday registry** (Hormiga: the `.miga`
  topology). A query-backed mantle (§10.2) names the holiday it resolves through.
- Distinguishes **owned** content (placed under a mantle, materialized in state)
  from **not-owned** data (reached via a holiday, never fully materialized).

### 10.2 Query-backed (lazy) mantle
A mantle whose runes are **not stored** but fetched on demand from a holiday,
filtered by a tag expression, at read/render time:
```jsonc
{ "name":"june-events", "backing": { "holiday":"events", "where":"@month:june AND type:event" } }
```
- `describe`/`ls` on a query-backed mantle resolve the query and summarize the
  *shape* of results (count, tags) without materializing every row into state —
  this is what lets an agent operate on the system without dumping the database.
- A block-rune MAY carry a `source` tag-query that resolves through a holiday at
  `render`/`save`, optionally **snapshotting** the resolved set into the owned
  newsletter at save time.

---

## 11. Conformance

An implementation is **reference-conformant** if, for every behavior marked
**[impl]** above, it:
1. preserves the data shapes in §3 (round-trips the §2 state document);
2. returns the §6 result shape and honors §6 mutation invariants
   (undo/redo/dirty-tracking);
3. implements §5 tag matching and the filter grammar exactly;
4. implements the §7 verbs with the stated semantics;
5. runs the §8 Voidscript **core subset** (the advanced constructs are oracle-only
   and not required);
6. routes all real-world effects through the §9 adapter + logging seam.

**[seam]** behaviors (the §7 transformation verbs) are part of the dispatcher
*contract* but OPTIONAL per implementation: an implementation MAY provide them, and
one that claims them MUST match the Python reference impl's semantics — purity (no
I/O / clock / RNG), preview-before-commit, and write-back through the undoable §7
verbs (`setjson`/`tag`) so every mutation stays one undo frame. They are verified
against the Python seam's test suite (`voidcore/dispatch_test.py` and siblings), not
`conformance/`.

`conformance/` holds language-neutral test cases (self-checking Voidscript
`assert` scripts — see `conformance/README.md`) that every implementation runs.
**[ext]** features (§10) are tested separately and only against implementations
that claim them (Hormiga).

---

## 12. Open spec questions
- Undo across holiday-backed data: the boundary between undoable owned-mantle
  edits and non-undoable external writes (§10) needs defining.
- Whether `layout`/`rules`/`relations` stay reserved or get a real consumer.
- Whether the fundamental tag axes are frozen or application-extensible.
- The exact `render`/`deploy` contract for non-file-backed domains (§3.5, §9).
