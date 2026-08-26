# The portable Scry projection contract

**What this is.** The Scry layer (the read side, SPEC §7 `[seam]`) lives at the Python
seam (`scry/`), deliberately outside the C core. A host in another language that needs
Scry therefore has to implement it — and one already has (`VoidMaiz/src/project/project.cpp`).
This directory is the **contract** that makes that safe: a language-neutral statement of
the semantics (§1–§5) plus pure-JSON conformance cases (`cases/*.json`) any implementation
can run.

The Python implementation (`scry/projection.py`) remains the **reference**: where this
document is ambiguous, the reference decides, and the fix is a new case pinning the answer.

Run the reference against the cases:

    python conformance/scry/run.py

Port `run.py` (~110 lines) to your language to test your implementation against the same
`cases/*.json`. See `conformance/temper/README.md` for why these two directories exist.

## 1. What Scry is

Scry derives **views** from owned state and an explicit **context** — purely. Same inputs
⇒ byte-identical output, which is what makes live preview, final render, and archived
send the same thing.

**One law, checked by the runner on every case:** **purity** — the read side never mutates
what it read, and never reads a clock, a locale, or an RNG. Everything environmental
arrives in the `Context` (`locale`, `audience`, `date`, `role`, plus a free `extra` bag).
A projection that calls `now()` is not conforming, however convenient.

**Identity is the default:** an empty selector returns every rune in input order.

## 2. `where` — the tag grammar

`where` is the SPEC §5 tag-filter grammar, and it MUST agree with the C core's
`vc_filter_eval` exactly:

```
or   := and ( ("OR"|"||") and )*
and  := not ( ("AND"|"&&") not | <atom> )*     # adjacency is an implicit AND
not  := ("NOT"|"!") not | atom
atom := "(" or ")" | TAG
```

Operators are case-insensitive; an empty expression matches every rune. A rune matches a
TAG when **any** of these hold — the two reserved forms are the ones ports miss:

1. the tag is in the rune's `tags` array;
2. the tag equals the rune's **`spirit.name`** (a rune is addressable by name in any tag
   expression);
3. the tag is `glyph:<the rune's glyph>`.

> **Do not re-implement this.** The C ABI exports `vc_tag_match(expr, tags_json)` precisely
> so that hosts filtering their own entities get the one tested implementation
> (SPEC §5 mandates evaluating through the seam). Cases 02 and 03 are here to catch a port
> that re-implemented it anyway — which is the situation that produced them.

## 3. `scry` — filter, sort, limit

The data-expressible selector is `{where, sort, reverse, limit}`. Applied **in this order**,
and the order is observable (case 04):

1. **filter** by `where`;
2. **sort** by `sort` — a *content field name*; a rune missing that field sorts by its
   `spirit.name` instead (not null, and never dropped). `reverse` reverses the comparison;
3. **limit** — take the first `limit` of the *final* order. Limiting before sorting is the
   classic port bug and yields a different answer;
4. **select** — a projection function. This one is **code, not data**: it is an arbitrary
   `(rune, context) -> value` mapping, so it has no JSON form and is out of this contract.
   Everything above never alters a rune, which is why `scry` cases pin the resulting
   **sequence of names** rather than whole runes.

**Deliberately not in this contract (v0.1):** `dedupe_by` (context-aware variant selection)
takes two callables — a grouping key and a preference ranking. Expressing it as data would
mean inventing a small language, and no port has asked for it. If one does, it gets a case
format and cases, the way this suite grew from the Reduce one.

## 4. `provenance` — the byte-level commitment

`provenance(value)` is the snapshot id of a JSON value, and unlike everything else here it
is a claim about **exact bytes**:

    canonical  = JSON with object keys sorted, no whitespace (`,` and `:` separators),
                 non-ASCII emitted as UTF-8 rather than \u escapes
    provenance = first 16 hex chars of SHA-256(canonical, UTF-8)

So `provenance({}) == "44136fa355b3678a"`, which is `sha256("{}")` truncated — a two-line
check that your encoder agrees before you debug anything else.

Key order does not matter (that is the point); nesting sorts at every level; empty object,
empty array and null are all distinct.

> **The float hazard, pinned in case 07.** `{"n": 1}` and `{"n": 1.0}` hash **differently**,
> because they are different JSON text — and languages disagree about which one a numeric
> value serializes to (Python writes `1.0`, JavaScript writes `1`). Void Core does not
> normalize numbers, so **avoid non-integer numbers in stamped data**, or agree a
> normalization with your peers *outside* this contract. Void Palabra's canonical byte
> encoding (tagged + length-prefixed) is the principled fix for that class; this hash is
> the older, narrower commitment and only claims what §4 says it claims.

## 5. Case format and error kinds

```jsonc
{"case": "NN-name",
 "description": "what this pins",
 "op": "scry" | "tag_match" | "materialize" | "provenance",
 "input": {"runes": [ ... ]},          // all ops except `provenance`
 "selector": {where, sort, reverse, limit},   // op: scry
 "context":  {locale, audience, date, role, extra},  // op: scry
 "expr": "<tag expression>",           // op: tag_match
 "resolved": {"<rune name>": {field: value}},        // op: materialize
 "into": "content" | "tags",           // op: materialize (default "content")
 "stamp": "<content field>",           // op: materialize, optional
 "values": [ ... ],                    // op: provenance
 "expect": {...}                       // shape depends on op; or {"error": "<kind>"}
}
```

Expectation shapes: `{"names": [...]}` for `scry`/`tag_match`, `{"runes": [...]}` for
`materialize`, `{"ids": [...]}` for `provenance`. Results are compared as canonical JSON,
so key order in a fixture is not significant.

`materialize(runes, resolved, into, stamp)` freezes resolved values into owned state — the
one explicit bake, kept non-automatic on purpose (silent bake-into-state is the bug class
it avoids). A rune with no entry in `resolved` passes through untouched and **unstamped**.
`into: "content"` merges over existing content; `into: "tags"` appends `field:value` tags,
skipping any already present so re-materializing does not grow the list. `stamp` writes
`content[stamp] = provenance(that rune's resolved fields)` — always to `content`, even when
the values went to `tags`.

| error kind | when |
|---|---|
| `bad-into` | `materialize` given an `into` other than `content` / `tags` |
| `bad-selector` | a selector spec that is not an object |
| `unknown-op` | the case names an `op` the runner does not implement |
| `law-purity` | reported by the runner: the input runes were mutated |

## 6. Authoring new cases

Write the inputs, run `python run.py --regen`, **eyeball the generated `expect` against the
semantics above**, commit. The reference is the oracle, but a golden file is only as good
as its review.
