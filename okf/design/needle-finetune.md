---
type: Design
title: Needle fine-tune — a resident micro-agent for Void Core
description: Plan for synthetic data creation to fine-tune the 26M-param needle function-calling model into a default, on-device NL→dispatcher seam for Void Core applications.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-13T00:00:00Z
---

# Needle fine-tune — a resident micro-agent for Void Core

> **The idea.** [needle](https://github.com/cactus-compute/needle) (cloned at
> `../needle`) is a 26M-parameter, MIT-licensed, on-device function-calling model.
> Void Core's dispatcher is already a function-calling surface — ~40 verbs, one
> result contract, a reserved `agent` namespace (SPEC §7). Fine-tune needle so
> that natural language routes to dispatcher commands, and every Void Core app
> gets a local, offline, instant "command bar brain" as a *default core
> component* — no cloud model in the loop for the routine 90%.

---

## 1. What needle actually is (verified against the source)

- **Architecture:** "Simple Attention Network" — encoder-decoder, attention-only
  (no FFN). 12 encoder layers, 8 decoder layers, 512-dim, GQA + RoPE. Distilled
  from Gemini 3.1.
- **Task:** *single-shot* function calling. Input = user query + JSON array of
  tool schemas; output = JSON array of calls
  `[{"name":..., "arguments":{...}}]`.
- **The authors' own framing of why it works:** tool calling is
  *retrieval-and-assembly* — match query to tool name, extract argument values,
  assemble JSON. "All three are aligning and **copying** between input and
  output — exactly what cross-attention does."
- **Hard limits** (`needle/dataset/tokenizer.py`, `needle/model/run.py`):
  - encoder input `[query…, <tools>, tools…]` truncated at **1024 tokens**;
  - decoder output max **512 tokens**;
  - one shot: no multi-turn state, no planning loop, no chain-of-thought.
- **Fine-tune interface:** JSONL rows `{query, tools, answers}` (tools/answers
  are JSON-encoded strings); `needle finetune data.jsonl` or the playground UI.
  Recommended **≥120 examples per tool** (100 train / 10 val / 10 test), varied
  phrasing, with multi-tool distractors present.
- **Extras we can use:** constrained decoding is built in
  (`generate(..., constrained=True)`), and the docs describe an optional
  **CLIP-style tool-retrieval head** (rank tools by cosine similarity, take
  top-k) for large tool sets.

**Design consequence, stated once:** this model does not reason, plan, or
converse. It *copies and routes*. Every choice below exists to reshape Void Core
tasks into copy-and-route form, and to keep anything requiring actual reasoning
out of its job description.

---

## 2. Why the fit is real

| needle needs | Void Core already has |
|---|---|
| A closed set of named tools with typed args | The verb catalog (SPEC §7): closed, documented, stable semantics |
| Machine-checkable outputs | `{ok, lines, data}` + a **conformance suite** + a real core to execute against |
| One-shot calls, no dialogue | Dispatcher verbs are one-shot by construction; multi-step = `batch` (atomic, one undo frame) |
| Cheap recovery from wrong calls | Everything mutating is **undoable**; the risky stuff is fenced at the holiday boundary |
| A place to live | The reserved `agent …` namespace — "the LLM seam — not wired" (SPEC §7.2) |
| A tool-retrieval story at scale | "Scaling to thousands is a *retrieval* problem — tools are taggable (reuse the tag system!)" ([agent-tools-memory](/design/agent-tools-memory.md) §1) |

Void Node strengthens the case: it turns every gesture into a *visible
dispatcher command*, so an NL command bar in Void Node emits exactly the same
artifact a wire-drag does. One log, one attribution stream (`config set actor
needle`), human and model edits indistinguishable in kind.

---

## 3. The central mapping: verbs → tools

**Decision: one needle tool per verb *operation*, snake_cased, dispatcher-faithful.**

- `rune new` → `rune_new`, `rune rename` → `rune_rename`, `tag` → `tag_edit`,
  `ls` → `ls`, `set` → `set_field`, … Needle routes on *name matching*; flat,
  specific names play to that. (needle even ships `to_snake_case`.)
- Arguments mirror the verb's positional/flag shape. A ~30-line deterministic
  **desugarer** converts the JSON call back to a canonical argv line
  (`{"name":"tag_edit","arguments":{"ref":"@group:science","add":["status:done"]}}`
  → `tag @group:science +status:done`). Lossless, testable, no model involved.
- **Do not** teach the POSIX aliases as separate tools. Aliases are input
  spellings for humans; the model targets canonical verbs only (aliases MUST NOT
  fork behavior anyway — SPEC §7.1). The *mental model* behind the aliases
  (mantle ≈ directory, rune ≈ file, tag expression ≈ glob) is however exactly
  the right intuition to encode in tool descriptions and query phrasings.

**The 1024-token budget forces tiering.** ~40 full schemas do not fit alongside
the query and state digest. Two-stage design:

1. **Surface** a subset of tools per call — either the CLIP retrieval head, or
   (more Void Core-native) tag the tools by axis and pre-filter with
   `vc_tag_match`; start dumb with a fixed "core 12" + intent-keyword expansion.
2. **Generate** against the surfaced subset (8–12 slim schemas ≈ 400–600
   tokens, leaving room for query + state digest).

Training data must match inference: every row carries a *plausible surfaced
subset* including distractors, never the full catalog, never only the right
answer.

---

## 4. The state-grounding problem (the big divergence from weather demos)

In `get_weather("Paris")`, the argument is *in the query*. In Void Core, the
arguments are **references into live state**: rune names, mantle names, glyph
names, existing tags. "tag the draft posts as reviewed" is unanswerable without
knowing that a `status:draft` tag exists and what the active mantle is.

**Decision: a canonical, compact state digest travels with every call**, both at
inference and in every training row. Draft shape (budget ≈ 150–250 tokens):

```
[mantle biology-hub] [domain biology-portfolio]
runes: intro(text) methods(text) results(bubble) refs(link)
tags: group:science status:draft status:published month:june
glyphs: text richtext image bubble dialogueLine
```

- Built by the host from `where` + `ls` + `axes` + `glyphs` — all read verbs,
  all already in the contract. Void Node's projection engine computes this
  anyway.
- On overflow (hundreds of runes), the digest lists *tags and counts*, not rune
  names — which is exactly the query-backed-mantle discipline of SPEC §10.2
  ("summarize the shape of results without materializing every row"), and it
  nudges the model toward `@tagexpr` bulk targeting instead of per-rune calls.
- The skill being trained is **copying names out of the digest** into arguments
  — cross-attention's home turf. Synthetic data must therefore vary digests
  aggressively (names, orderings, sizes) so the model learns *copy from
  context*, not *memorize names*.

---

## 5. The concept curriculum — what to teach, in priority order

Tier 1 is the fine-tune's reason to exist; each later tier is added only when
the previous one evaluates clean.

### Tier 1 — the routing core (~must be flawless)
1. **Read-verb routing:** `ls`, `describe`, `get`, `find`, `cat`, `status`,
   `tree`, `where`, `links` — "what's here / show me X / did anything change".
   High volume, zero risk, the trust-builder.
2. **Tag filter expressions** — *the single highest-value compositional skill.*
   `namespace:value`, AND/OR/NOT (word and symbolic forms), parentheses,
   implicit AND, name-as-tag, `glyph:<name>`. This is Void Core's glob; it is
   what turns one call into a bulk operation. Deserves the largest slice of the
   dataset by far, with the conformance grammar (`01-tags.vs`) as the coverage
   checklist.
3. **`@expr` multi-targeting:** "mark all science drafts published" →
   `tag_edit(ref="@group:science AND status:draft", add=["status:published"], remove=["status:draft"])`.
   One call, N runes — the idiom that makes a single-shot model *feel* powerful.
4. **Reference discipline:** name vs `spirit.id` both valid; `@` means many;
   plain name means one; when the query is ambiguous between a rune and a tag,
   prefer the reading the digest supports.

### Tier 2 — safe mutation
5. **Field & facet writes:** `set` vs `setjson` (typed/structured values go
   through `setjson`), `facet` with its closed enum
   (who/what/when/where/why/how — reject "set the mood facet").
6. **Tag editing syntax:** `+add -remove`, multiple in one call.
7. **Lifecycle of runes/mantles:** `rune_new` (glyph must come from the
   digest's registered set), `rune_rename`, `rune_dup`, `rune_rm`, `mantle_new`,
   `use`.
8. **Links vs bindings:** `link/unlink` (passive, may dangle) vs `bind/unbind`
   (reactive, validated) — the vocabulary distinction Void Node paints as
   linguine vs fettuccine. The model mostly needs to *pick the right one from
   the query's verb* ("connect/relate" → link; "when X happens do Y" → bind).

### Tier 3 — plans without planning
9. **`batch` as the plan container:** a multi-step intent ("make a June
   newsletter section: new group rune, tag it, link it under intro") becomes
   *one* `batch` call with an ordered command array — atomic, one undo frame.
   This is the honest ceiling for a single-shot model: it can emit a *static*
   plan whose steps don't depend on intermediate reads. Anything requiring a
   read-then-decide loop is **out of scope by design** (that's the big-model or
   Voidscript tier).
10. **Transform verbs, invocation only:** `scry` (tagexpr / `--select`),
    `temper [<ref>]`, `reduce` (and that `--commit` is the separate, explicit
    step). The model routes *to* them; it never composes reducer rules.

### Tier 4 — the refusal class (trained, not hoped for)
11. **Escalation as a first-class answer.** A dedicated `defer` tool
    (arguments: `reason`, optionally the draft command) that the model is
    *trained to emit* for: `deploy`, `revert`, `effect *`, `import`,
    `config set` beyond actor, anything outside the surfaced tool set,
    genuinely ambiguous references, and queries that need multi-step
    read-then-decide. A 26M model **will** be wrong sometimes; the design
    stance is that outward/irreversible ops are never one model call away —
    matching [agent-tools-memory](/design/agent-tools-memory.md) §3: all
    sandbox effort concentrates at the holiday boundary. ~10–15% of the
    dataset should be defer-labeled examples.

### Explicit non-goals for the fine-tune
- Voidscript *authoring* (loops, conditionals, functions) — that is generation,
  not routing. Voidscript stays the human/big-model tier; needle may *invoke*
  saved scripts (`script run <name> args`) as just another tool.
- Reducer/temper rule authoring, glyph design, conversation, explanation.
- Undo-stack reasoning ("undo the thing I did before lunch") — `undo`/`redo`
  with counts route fine; temporal reasoning does not.

---

## 6. Synthetic data pipeline

The unfair advantage: **Void Core can execute the labels.** Unlike generic
function-calling data, every candidate `(digest, query, answer)` triple can be
validated against a live core.

```
grammar templates ──┐
                    ├─→ candidate rows ─→ EXECUTE against C core ─→ keep/fix/drop
random state gen  ──┘         │              (assert ok + postconditions)
                              └─→ LLM paraphrase of queries (n variants each)
```

1. **State generator:** random-but-plausible mantles (rune counts 0–200, tag
   vocabularies drawn from themed pools per app archetype: site manager,
   newsletter, DAW, study tool). Emit the digest *and* keep the live core
   instance.
2. **Intent templates per tool:** each tool gets parameterized intent seeds
   ("tag every {tagexpr-description} as {tag}", "what {glyph} runes are in
   {mantle}?"). The template also emits the gold answer *symbolically*, filled
   from the generated state — so the gold argument is guaranteed to reference a
   real name (or deliberately not, for `defer` rows).
3. **Oracle validation:** desugar the gold answer to argv, dispatch it on the
   live instance, assert `ok:true` plus a postcondition (the tag is present,
   the field equals the value, `ls --tag` returns the expected set). Rows that
   fail are bugs in the generator — fix, don't filter silently. This reuses the
   exact discipline of `conformance/` (self-checking assert scripts), and the
   conformance cases themselves are the first seed corpus.
4. **Query paraphrasing:** a big LLM rewrites each template query n ways
   (terse, verbose, typo'd, POSIX-flavored — "rm the draft bubbles", jargon-free
   — "get rid of the unfinished speech balloons"). Paraphrases are cheap;
   *state variety and expression coverage* are the scarce resources — budget
   accordingly.
5. **Distractor discipline:** every row's `tools` array = gold tool + 5–10
   plausibly-surfaced distractors (per §3's tiering); include rows where the
   gold action needs a tool that is *not* surfaced → gold answer is `defer`.
6. **The log as free real data:** the mutation spine logs every successful
   mutating command verbatim, with attribution (SPEC §9). Real sessions from
   Portfolio Manager / Fountain / VLS yield `(state-before, command)` pairs;
   only the NL query needs back-filling (LLM writes "what would a user have
   said to want this command here"). This becomes the *distribution anchor* so
   synthetic data doesn't drift from how the tools are actually used.

**Volume estimate:** Tier 1–2 ≈ 25 tools × 120 = 3,000 minimum, but tag-expression
compositionality wants dedicated depth — realistic first target **10–15k rows**
(~40% tag-expression-bearing, ~15% defer, ~10% batch), regenerable from seeds at
any time. That is small enough to fully regenerate on every curriculum change.

---

## 7. Base model + app layer (the "default aspect of Void Core" story)

The `120/tool` rule composes with the open-application design:

- **Base checkpoint** (`needle-voidcore-base`): the SPEC §7 verb catalog, tag
  grammar, generic digests. Trained once, shipped with the core, app-agnostic.
- **App layer:** an application brings registered glyphs, its own tag
  vocabulary, and its ~40 curated task-level tools
  ([agent-tools-memory](/design/agent-tools-memory.md) §1 — tools are
  Voidscript bodies over verbs, i.e. *also just named callables with args*:
  needle-shaped by construction). The app runs the same pipeline (§6) over its
  own tool manifest to produce its fine-tune on top of the base.
- The **app manifest** ([app-manifest-design](/design/app-manifest-design.md))
  is the natural place to declare the tool schemas + intent seeds, making
  "generate my app's needle dataset" a core-provided command. Long-term this is
  part of the app-instantiation standard: instantiate an app, get its resident
  micro-agent with it.

---

## 8. Shipping it as a core component

- **Wire point:** the reserved `agent` verb. `agent "<natural language>"` →
  host builds digest → surfaces tools → needle call → desugar → dispatch → the
  result is the dispatched verb's own `{ok, lines, data}` plus an `agent`
  echo of what was run. Implement at the Python `voidcore.Dispatcher` seam
  first (same [seam] discipline as the transform verbs); C-side later if ever.
- **Attribution:** the seam sets/propagates `config.actor` so every
  model-issued mutation is stamped (`history` shows `[needle]`) — the who-
  attribution work from 0.2.3 makes model edits auditable for free.
- **Preview-before-commit for mutations:** the UI affordance is "show the
  desugared command, run on Enter" (Void Node: the command appears in the log
  strip *as a proposal*). Reads can auto-run; mutations show their one-liner
  first. Undo covers the rest.
- **Constrained decoding:** needle's `constrained=True` already forces valid
  JSON-call shape; a Void Core grammar layer on top (valid verb names, facet
  enum, flag names) turns most residual hallucination into hard decode-time
  impossibility. Grammar-level safety, exactly the PEL lesson.
- **Capability = surfaced tool set:** disabling a verb for a seat/capsule means
  not surfacing its tool — the model *cannot* call what it cannot see, and the
  desugarer rejects anything off-list as defense in depth.
- **Runtime:** needle is JAX; for "default aspect of Void Core" it needs a
  lean inference path (export to ONNX/GGML-class runtime, or cactus's own
  mobile runtime). Unresolved; does not block data work, which is
  runtime-agnostic.

---

## 9. Evaluation

- **Exact-match on canonical argv** (after desugar) for routing/copy fidelity.
- **Execution success:** dispatch on a live core, assert `ok` + postconditions
  — the same oracle as §6, on held-out states *and* held-out intent templates
  (generalization across both axes measured separately).
- **Defer precision/recall** tracked as its own headline metric: a false
  "confident call" on a defer-labeled row is the worst failure class.
- **Distractor robustness:** accuracy as surfaced-set size and similarity
  grows (e.g. `rune_rm` vs `unlink` vs `rune_rename` all present).
- Regression gate: the conformance-seeded slice must stay at ~100% — it is the
  contract, not just data.

---

## 10. Open questions

1. **Tool surfacing v1:** fixed core-12 + keyword expansion, or train the CLIP
   retrieval head from day one? (Leaning: start fixed, measure, then retrieve.)
2. **Digest canonical form** — freeze the format early; it is a *contract* the
   base checkpoint bakes in. Where does it live — SPEC extension or seam doc?
3. **Multi-call outputs:** needle emits an *array* of calls — allow ordered
   multi-call answers directly, or force everything multi-step through `batch`?
   (Leaning: `batch` only, keeps atomicity + one undo frame.)
4. **How much Voidscript-invocation** (saved `script run`) belongs in the base
   vs app layer?
5. **Quantify the digest-size ceiling:** at what rune/tag count does accuracy
   fall off, and does the tags-and-counts fallback (§4) actually hold it?
6. **Runtime for embedding** (§8) — JAX won't ship inside a C library.
