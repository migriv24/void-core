# The portable Temper normalization contract

**What this is.** The Temper layer (normalization, SPEC §7 `[seam]`) lives at the Python
seam (`temper/`), deliberately outside the C core. A host in another language that needs
Temper therefore has to implement it — and two already have. This directory is the
**contract** that makes that safe: a language-neutral statement of the semantics (§1–§4)
plus pure-JSON conformance cases (`cases/*.json`) any implementation can run.

The Python implementation (`temper/temper.py`) remains the **reference**: where this
document is ambiguous, the reference decides, and the fix is a new case pinning the
answer.

Run the reference against the cases:

    python conformance/temper/run.py

Port `run.py` (~90 lines) to your language to test your implementation against the same
`cases/*.json`.

> **Why this exists.** Void Palabra observed (2026-07-27) that of the three transform
> layers only Reduce had a portable contract — and that this is exactly the difference
> between being *ported* and being *reinvented*: Void Maiz ported Reduce against
> `conformance/reduce/` and checked it, while Void Hormiga rebuilt Temper from the
> concept page (`VoidHormiga/src/temper.hpp`) because there was nothing to check against.
> Two hand-written copies of a normalization pass will drift, and without case files the
> drift is silent. Nobody did anything wrong; the contract was missing.

## 1. What Temper is

A **rule** is a pure function `rune -> rune`. A **Temper pass** is an ordered list of
rules, applied to each rune in turn:

    temper(rune) = rule_n( ... rule_2( rule_1(rune) ) ... )

Temper canonicalizes **owned state** after an action: derived-field defaults, de-duplication,
tag normalization. It is *context-blind* — no locale, no audience, no clock. (A
context-dependent tiebreak belongs in Scry, on the read side; see
`conformance/scry/README.md`.)

**Two laws, checked by the runner on every case** (not only where a case asks):

- **Idempotence** — `temper(temper(x)) == temper(x)`. This is the layer's reason to
  exist: applying it twice must be indistinguishable from applying it once, so a host may
  run it after every mutation without accumulating change.
- **Purity** — the input runes are not mutated. `temper` returns new objects; no I/O, no
  clock, no RNG.

An implementation that matches every expected output but breaks a law is **not
conforming**, and `run.py` reports `law-idempotence` / `law-purity` rather than a diff.

**Identity is the default:** an empty rule list returns each rune unchanged, including
runes with absent `content`/`tags` containers. Temper never invents structure it was not
asked for.

## 2. The rule set (data form)

Rules arrive as data, in the `config.transform.temper` form of the state document — a
list of objects, each naming a `rule` plus that rule's arguments:

```jsonc
[{"rule": "dedupe", "field": "images"},
 {"rule": "member_or_default", "target": "thumb", "source": "images"},
 {"rule": "default_tag", "namespace": "status", "value": "complete"},
 {"rule": "single_tag", "namespace": "status"},
 {"rule": "normalize_tags", "sort": false}]
```

| rule | required | optional | semantics |
|---|---|---|---|
| `dedupe` | `field` | | De-duplicate the content list `field`, **preserving first-seen order**. A field that is absent, null, or not a list is left untouched — never coerced. |
| `member_or_default` | `target`, `source` | `index` (0), `empty` (null) | `content[target]` must be a member of the list `content[source]`. If it already is, **leave it alone**; otherwise reset it to `source[index]`, or to `empty` when the source list is empty. A non-list `source` is left untouched. |
| `default_content` | `field`, `value` | | Set `content[field]` to `value` when it is missing, null, or the empty string. |
| `default_tag` | `namespace`, `value` | | If **no** `namespace:*` tag is present, append `namespace:value`. A default, not an override. |
| `single_tag` | `namespace` | | Collapse a single-valued axis: if several `namespace:*` tags are present, keep the **first** and drop the rest. |
| `normalize_tags` | | `sort` (false) | De-duplicate `tags` preserving order; with `sort: true`, sort bytewise instead. A non-list `tags` is left untouched. |

Two details worth stating because they are easy to get wrong, and each has a case:

- **Order is observable.** A pass is an ordered pipeline, not a set of constraints solved
  to a fixed point. `dedupe` then `member_or_default` validates the pointer against the
  de-duplicated list; the reverse order does not. An implementation that groups by rule
  kind, sorts the rules, or iterates to convergence will disagree (case 05).
- **`member_or_default` keeps a valid pointer.** Always resetting to `source[index]` passes
  the simple cases and fails case 03 — and in a real app it silently overwrites the user's
  chosen thumbnail on every save.

## 3. Case format

```jsonc
{"case": "NN-name",
 "description": "what this pins",
 "spec": [ {rule objects} ],           // §2 data form
 "input": {"runes": [ ... ]},          // runes as they appear in the state document
 "expect": {"runes": [ ... ]}          // or {"error": "<kind>"}
}
```

Runes are compared as **canonical JSON** (object keys sorted, no whitespace), so key
order in a fixture is not significant. Rune order **is** significant: a pass maps runes
positionally and never reorders them.

## 4. Error kinds

Abstract; each implementation maps them to its own error type.

| kind | when |
|---|---|
| `unknown-rule` | the spec names a rule the implementation does not know |
| `missing-arg` | a known rule is missing a required argument (§2) |
| `law-idempotence` | reported by the runner: a second pass changed the result |
| `law-purity` | reported by the runner: the input runes were mutated |

**A spec must be rejected, never partially applied.** Skipping an unrecognized rule is
the dangerous failure mode: the pass reports success while the invariant it was supposed
to enforce never ran. Temper specs ride in the state document, so a bundle authored
against a newer implementation *will* reach an older one, and it must fail loudly
(cases 07, 08).

## 5. Authoring new cases

Write `spec` + `input`, run `python run.py --regen`, **eyeball the generated `expect`
against the semantics above**, commit. The reference is the oracle, but a golden file is
only as good as its review.
