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
  **`spirit.id` MUST NOT be derived from a rune's content.** Two runes that happen
  to hold identical content are different runes, and the randomness is load-bearing
  downstream: it makes two peers who independently create a same-named rune produce
  genuinely distinct runes with no conflict, and it labels the mantle's graph, which
  is what keeps content-addressing a mantle an `O(n log n)` sort instead of graph
  canonicalization. (Recorded at Void Palabra's request, 2026-07-27 — it looks like a
  tidy cleanup and would break both properties at once.) The one carve-out is agents
  minted *inside* a reduction, which are derived from the redex so that reduction is
  reproducible across peers — see `conformance/reduce/README.md` §2.
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
  "placement": null,                   // optional explicit position — the VIEW SLICE (§6)
  "relations": []                      // optional; reserved
}
```
- The six **facets** are always present (may be empty). They are uniform textual
  metadata so any rune can be described/reasoned about by a human or LLM.
- `content` is opaque to the core; only the rune's **glyph** (§3.3) interprets it.
- `placement` is the **view slice**: where a rune sits in a spatial view
  (`null`, or `{"x":n,"y":n}` / `{"x":n,"y":n,"z":n}`), written by the `place`
  verb (§7.2). It is model state (it serializes with the rune) but **not
  undoable** state — see §6. Apps MUST use `placement`/`place` for positions
  rather than smuggling them into `content`, so that undo never pops a *move*
  when the user expects it to pop an *edit*.
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

**Mantle-level lifecycle** (`mantle new|rm|rename`, §7.2) — the analogues of the
rune operations above, over `state.mantles`:
- `removeMantle` — drop the mantle and its runes. If it was the active mantle,
  set `active.mantle` to `null` (the §7.1 cold-start state) rather than
  failing; removing the **last** mantle is allowed, since root-`ls` already
  describes an empty mantle list. The name becomes free for reuse.
- `renameMantle` — keep the mantle's `id`, `runes`, `layout` and `rules`; reject
  a taken name (as `mantle new` does); carry `active.mantle` to the new name.
- Both mutate the undoable slice (`mantles` + `active`, §6) and so take a normal
  undo frame — an undone `mantle rm` restores the mantle, its runes, and the
  active pointer.
- Neither touches `state.bindings`: a binding into a removed or renamed mantle
  is left **dangling**, the same tolerance §3.7 gives links. This is deliberate —
  `bindings` is outside the undo slice, so repointing them here would produce a
  mutation that `undo` could only half-restore. See §12.

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
- **Rune order is preserved but not semantic.** A mantle's `runes` is an array, and
  implementations MUST preserve its order faithfully — creation appends, `rename`
  keeps a rune's position, `rm` closes the gap, `undo` restores positions, and
  `export`/hydrate round-trip it. But **no verb's semantics depend on that order**:
  `ls` does not sort, and filters (§5) never reorder. Order is therefore *incidental
  information that is faithfully carried*, not meaning. Two consequences: an
  order-sensitive consumer (a canonical form, a content hash, a sync join) MUST be
  order-insensitive at the Core level, because two peers who create the same runes in
  different orders hold **equal** state; and an application that genuinely needs an
  ordering MUST make it explicit in a content field, never lean on array position —
  the same lesson as `placement` in §3.2.

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
  Tokenization: only whitespace and parentheses split a word; `&&`, `||`, `!` are
  operators **at a token boundary only** — mid-word they are ordinary tag
  characters (`a&b` is one TAG atom, not `a AND b`), so a stray `&`/`|` is a
  never-matching tag, never an error or a crash.
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
- argv is split respecting single/double quotes (the exact rules are §6.1 — a host
  that stores free text through the dispatcher MUST read them).
- `--flag` is boolean `true`; `--flag=value` or `--flag value` (for known
  value-flags: `tag`, `level`, `tail`, `message`/`m`, `state`, `port`, `as`,
  `name`, `note`, `mantle`) takes a value; `-x` is a short boolean (or value-flag).
- everything else is positional.

### 6.1 Argument quoting  **[impl]**

Normative, and stated at length because getting it wrong is **silent and corrupts
content rather than structure**: a mis-quoted argument terminates early, the rest of
the line is swallowed, and dispatch still returns `ok: true`. Five independent
implementations have now hit it — Void Hormiga 2026-08-10 in C++, Void Reyna
2026-08-17 in Python, the helper in Void Maiz's own example, Hormiga's host-side
`tokenize()`, and **this specification's own reference core**, which carried three
separate quote scanners (the Voidscript statement reader, the condition lexer and
the interpolator), none of which implemented rule 3 below. That last one is why
§6.1 no longer ships as prose alone: a rule that must be reimplemented will be
reimplemented wrong, and writing it down harder did not stop the fourth instance
or the fifth. Before 0.2.7 the only documentation was "argv is split respecting
single/double quotes."

The tokenizer's complete rules:

1. Outside quotes, **whitespace separates arguments**.
2. A bare `'` or `"` **opens** a quoted run and is *stripped*, never emitted. The
   matching quote **closes** it and is likewise stripped. Quoting is therefore
   *strip-anywhere*, not delimiting: `a'b'c` is the single argument `abc`.
3. Inside **single** quotes there is exactly **one escape**: `\'` yields a literal
   `'`. Every other backslash is literal — deliberately, so JSON payloads and text
   escape codes (`\n`, `\cY`) pass through untouched.
4. Inside **double** quotes there is **no escape at all**; the next `"` always
   closes. A literal `"` therefore cannot appear in a double-quoted argument — use
   single quotes, inside which `"` is an ordinary character.
5. A quoted run that is still open at end of input is an **error** (since 0.2.7).
   Dispatch returns `ok:false` and logs `ERROR`; a Voidscript statement halts the
   script. Until 0.2.7 it silently ran to end of input, and that single property
   is what made every bug in this class quiet: the argument swallowed the rest of
   the line — or, in a transcript, the rest of the file — and dispatch still
   returned `ok:true`. Nothing legitimate depends on the old behavior (a literal
   quote is spelled `\'`, or carried inside the other quote style), and the naive
   four-line helper's output is exactly what it turned into corruption.

**A newline inside a quoted run is ordinary content**, not a separator and not an
error. Rule 1's whitespace separates arguments *outside* quotes only, so a
multi-line value — a description, a newsletter body, a JSON blob — is carried by
a single argument, and `split(quote(v)) == [v]` holds for it like any other byte
string. Statement boundaries in a transcript (§8) are likewise honored only
outside quotes. A host that finds a multi-line value truncated has a bug above
the tokenizer, not in it.

**The law.** The codec is a section/retraction pair, and this is the sentence to
build against:

> A dispatcher argument carries an arbitrary **NUL-free byte string**, and
> `split(quote(v)) == [v]` for every such `v`.

NUL is excluded because the whole boundary is C strings — a value containing a
NUL cannot reach `vc_dispatch` at all — and saying so is part of the guarantee
rather than a gap in it. `voidcore/codec_test.py` pins the law as a **property
over generated inputs** rather than a vector list, because every failure in this
class so far has been a value nobody thought of.

**Both halves ship as code**, on the C ABI and in the Python binding, because a
rule that must be reimplemented will be reimplemented wrong — four independent
codebases have now proved it, this one included:

| | C ABI | Python |
|---|---|---|
| encode one argument | `vc_arg_quote` | `quote_arg` |
| decode one command | `vc_argv_split_json` | `split_args` |
| decode a whole transcript | `vc_transcript_split_json` | `split_transcript` |

The **decoder** matters as much as the encoder and is the half hosts forget. A
host that reviews a proposed transcript before dispatching it — a submission from
a stranger, a harvested dataset, an agent's proposed run — must be able to ask
*what will this text actually do* with the tokenizer that will do it. The
transcript splitter answers that: it cuts on newline and `;` **outside** quoted
runs, drops `#` comments, returns each statement's `argv`, refuses an
unterminated quote, and reports whether the transcript is **flat** (no blocks, no
§8 control words) — a flat transcript is one whose effect can be read off its
statements without simulating it. The Python halves are pure functions needing no
engine or build.

**To pass an arbitrary string as one argument**, single-quote it and replace every
`'` with `\'` — *and emit any trailing backslashes outside the closing quote*:

```
        arg(v) = "'" + v_head.replace("'", "\'") + "'" + v_tail
                 where v_tail is v's trailing run of backslashes, v_head the rest
```

That last clause is not a nicety. Without it a value ending in `\` puts a backslash
immediately before the closing `'`, rule 3 reads the pair as an escaped apostrophe,
and the argument never closes — so `C:\` silently becomes `C:'` and everything after
it is eaten. The naive four-line helper (single-quote, escape apostrophes) passes
every test a host is likely to write and fails on Windows paths, LaTeX, and regexes.

Reference implementation: `quote_arg` in `bindings/python/voidcore.py`
(`from voidcore import quote_arg`) — a pure string function requiring no engine, so
it may be imported or copied. Conformance case `12-arg-quoting.vs` pins the rules;
a host implementing the tokenizer must pass it.

A **JSON** value is passed with `setjson <ref> <field> '<json>'` — single quotes,
with `\'` for any apostrophe inside the JSON.

**Mutation invariants:** every mutating verb pushes an undo frame (snapshot of
mantles+active) *before* mutating; the redo stack is cleared on new mutation;
the undo stack is bounded (reference impl: 200).

**The view slice:** `rune.placement` (§3.2) is carved out of the undo slice.
`place` (§7.2) mutates it and is logged on the mutation spine (§9), but pushes
**no** undo frame, and `undo`/`redo` MUST NOT change the placement of a rune
that survives the operation (restoring a snapshot carries each surviving
rune's *current* placement over, matched by mantle name + rune name). A rune
that only exists in the restored snapshot (e.g. its removal was undone) comes
back with its snapshot placement. Exception: a failed `batch` rolls back
placements written inside it — atomicity of the failed batch outranks the
carve-out. Rationale: users undoing expect to pop *edits*, not *moves* (a
node-editor lesson); view state must also never pollute edit history.

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
| `rmdir <name>` | `mantle rm <name>` |
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
| `mantle rm <name>` | remove a mantle and its runes; removing the **active** mantle deactivates (§7.1 cold start) rather than failing, and removing the last mantle is allowed. Rejects an unknown name |
| `mantle rename <old> <new>` | rename a mantle, keeping its `id` and runes; `active.mantle` follows. Rejects an unknown `<old>` or a taken `<new>` (like `mantle new`) |
| `bind <from> <on> <to> <do> [--name]` | create a cross-mantle binding (validates refs) |
| `bindings [<rune>] [--mantle m]` / `unbind <id\|name>` | list / remove bindings |
| `undo [N]` / `redo [N]` | walk the undo/redo stacks |
| `batch '<json-array>'` | apply a JSON array of command strings **atomically** (rollback on any failure) as **one undo frame**. The payload is inline — the core does no file I/O (§9) |

**View (mutation spine, NOT undoable — the view slice, §6):**
| verb | meaning |
|---|---|
| `place <rune>` | read the rune's `placement` (`data` = value or `null`); a query — not logged |
| `place <rune> <x> <y> [<z>]` | set `placement` to `{"x":n,"y":n[,"z":n]}`; single rune (no `@`-multi); logged, no undo frame |
| `place <rune> --clear` | set `placement` back to `null`; logged, no undo frame |

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

### 8.1 Quoting and expansion  **[impl]**

Normative, and the part that was missing. A transcript is the channel every
change in the family passes through, and until 0.2.7 the interaction between
§6.1 quoting and Voidscript's own syntax was unspecified — which is where two
command injections lived.

1. **Statement boundaries are honored only outside quoted runs.** A newline, `;`,
   `{` or `}` inside a quoted argument is content. Quote state is §6.1's, escapes
   included — an implementation MUST use the same quote scanner for statements
   that it uses for argv. Two scanners is the bug, whatever their rules say.
2. **Single quotes suppress expansion.** `$var`, `${var}` and `$(cmd)` are
   literal text inside a single-quoted run, exactly as `\n` and `\cY` are (§6.1
   rule 3). Double quotes and bare words expand as before. Without this, a
   transcript built by correctly quoting somebody else's text runs whatever
   `$(…)` that text happens to contain — and a host's verb-level filter cannot
   see it, because the verb it reads is the legitimate one.
3. **An expansion is exactly one argument.** The bytes an expansion produces are
   content, never syntax: they are not re-scanned for quotes, separators or
   flags, and an empty expansion yields an explicit empty argument rather than
   disappearing. `let x = 'two words'` followed by `set r f $x` sets one field to
   `two words`, and a value containing `'`, a newline, `--json` or nothing at all
   behaves the same way.
4. **`${name}` is an expansion, not a block.** The brace belongs to the
   interpolator; a statement does not end at it.
5. **`let x = <rest>`** takes the rest of the statement — trailing *source*
   whitespace trimmed, since that is layout and not value — decoded as exactly
   one §6.1 argument, with no field splitting.
6. **`$(cmd --json)` captures the command's `data`**; a *string* `data` captures
   as itself, not as its JSON encoding. (Encoding it wrapped the value in quotes
   and escaped its contents, which downstream code then un-wrapped by accident
   and lossily — a captured newline came back as the two characters `\n`.)
   Non-string data still captures as JSON text.
7. **No length caps.** Statements, arguments, interpolated text, result lines and
   log messages grow as needed. A fixed buffer here truncates mid-UTF-8 sequence,
   which turns ordinary long content into invalid encoding rather than merely
   short content.

Conformance case `13-transcript-safety.vs` pins every clause above.

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
- **Attribution (`who`)** — session-scoped, via the config tier: when
  `config.actor` is a non-empty string (`config set actor <name>`), every log
  record carries a `who` field (line form `[ISO] LEVEL op (who): message`) and
  every undo frame is stamped with the actor at capture time (`history` shows it
  as a `[who]` suffix; `history`'s `data` stays a plain label array). With agents
  and humans sharing one dispatcher seam, the actor says which. Unset/empty actor
  = no `who` (fully backward compatible). The host log-sink callback signature is
  unchanged; sinks that need attribution read it from `log` records.
- **The mutation spine** — every successful top-level mutating command is logged
  (`INFO`, op = the verb, msg = the full command), so the log is a complete,
  attributable record of what changed the state. `batch` logs once (its
  sub-commands are inside that frame); `undo`/`redo` log their own application.
  View-slice mutations (`place`, §6) are on the spine like any other mutation —
  attributable, auditable — despite taking no undo frame.
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
- **Scope of the undoable slice.** It is `mantles` + `active` today, which leaves
  `bindings` outside it — so `mantle rm`/`rename` (§3.4) cannot repoint or drop
  cross-mantle bindings without creating a mutation `undo` only half-restores.
  Either bindings join the slice (and `bind`/`unbind` become undoable, which they
  are not today), or dangling bindings become a `validate` diagnostic like
  dangling links. Related: whether `config`/`domains`/`scripts` belong in the
  slice at all.
  **Reframing (Void Palabra, 2026-07-27):** *what a local user can take back* and
  *what may be sent to another machine* are two different questions, and forcing one
  answer on both is probably what makes this hard. Palabra had to answer the second
  and got a strictly narrower slice — `mantles` only, with `domains`/`bindings`/
  `config`/`active` as **peer-local resolution** that never syncs. Their forcing
  argument is not aesthetic: a domain carries real `build`/`deploy` commands (§3.5),
  so syncing one as content would execute one device's deploy command on another
  device. A mantle should reference a domain *by name* and each peer resolve it
  locally — the same reason a git remote's filesystem path is not cloned. Core's undo
  slice may keep `active` even though a sync slice must not; noticing that they are
  allowed to differ is the useful move.
- Whether `layout`/`rules`/`relations` stay reserved or get a real consumer.
- **`layout.edges` endpoints are mutable names** (§3.7), which `rename` repoints
  (§3.4). Coherent for one user; across peers, two who concurrently rename a rune and
  add an edge to it produce edge sets that cannot be reconciled by name alone. Noted,
  not scheduled — Void Palabra (2026-07-27) reports their OR-Set join can handle it by
  resolving names to `spirit.id` before comparing, and asked for no change. If link
  storage is ever revisited, `spirit.id`-keyed endpoints would remove the ambiguity at
  the cost of a migration.
- Whether the fundamental tag axes are frozen or application-extensible.
- The exact `render`/`deploy` contract for non-file-backed domains (§3.5, §9).
