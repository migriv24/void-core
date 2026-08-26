---
type: Design
title: The effect seam & the session boundary
description: Four open contract questions routed Core-ward by Void Maiz (2026-08-18) — can an effect handler fail a dispatch, should `save` move `_baseline` when the adapter didn't, is history session-scoped by design, and should the derived-id hash be normative. Parked with a stance, not yet decided.
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-08-25T00:00:00Z
---

# The effect seam & the session boundary

> Four questions from the Void Maiz agent, 2026-08-18, arriving out of a headless
> front-end: an agent drives the application from a terminal, the changes land in
> the same state document a GUI opens, and a person reviews them later. Nothing
> here blocked them — they built *around* all four — which is exactly why it is
> worth deciding rather than leaving as folklore. **Parked deliberately: these are
> contract changes to §9 and §2, and they deserve more review than the §6.1 codec
> work of 0.2.7 did.** Their own ranking: if only one gets attention, make it §1(b).

Filed here rather than in `SPEC.md` because none of it is decided yet. When one
is, it graduates to SPEC and out of this note — the convention
[host extension seams](/design/host-extension-seams.md) already follows.

## 1. The effect seam (their item 2b) — the one with a backward-compatible fix

Context they were building: an **effect gate**. A headless agent can do anything a
dispatcher command can do, and all of it is reversible through `_baseline` — *until
it reaches an effect*. A deploy pushes a live site; a save writes a real backend.
So effects are refused by default and a person grants them per run. Building that
gate turned up two behaviours they designed around.

### (a) `save` snapshots the baseline even when the adapter failed

`core/src/dispatch/verbs_lifecycle.c`:

```c
} else if (!strcmp(v, "save")) {
    res = res_make(1);
    if (m->effect) { char *hr = m->effect("save", st, m->effect_user); ... }
    vc_snapshot_baseline(state);        /* unconditional */
```

If the host's save adapter cannot write — disk full, network down, permission
denied — `_baseline` moves anyway. The diff that said *"these changes are not yet
persisted"* is gone, the user is told they are in sync with a backend that never
received the data, and `revert` will discard the only remaining copy.

**The question is what `_baseline` means**, and we have never said:

- *"last model-side checkpoint"* — then this is correct, and surfacing the
  adapter's failure is purely the host's problem. Maiz would document it that way.
- *"last state that actually reached the backend"* — then this is a data-loss path.

**Stance (undecided, leaning second).** `status`/`diff`/`revert` are how a host
presents "what has not been persisted"; if `_baseline` can move without persistence
having happened, that presentation is a lie in exactly the case where it matters.
Their minimal fix changes no existing behaviour: snapshot only when the adapter did
**not** report failure — skip it if the handler returns a JSON object with
`"ok": false`. Handlers returning `NULL` or anything else keep today's semantics
byte for byte.

### (b) An effect handler cannot fail a dispatch — **their top ask**

`res = res_make(1)` is set *before* the handler runs, for `save`, for
`deploy`/`build`/`preview`, and for the generic `effect` verb. Whatever the handler
returns becomes `data`; nothing it can return makes `ok` false.

So **a host cannot report that an effect failed.** A deploy that could not reach the
server, an adapter that refused, a build that did not compile — every one comes back
as a successful command with an error message buried in `data`. Maiz's framing is
the right one: *"For a human at a REPL that is survivable, because they read the
line. For an agent or a shell script branching on `ok`, it is not — the failure is
invisible at exactly the layer that automates."*

It also cost them their design. They wanted "refuse this deploy" to mean *did not
happen* **and** *reported as failed*; because of (a) they could not gate at the
handler (the baseline would move anyway), and because of (b) a handler-level refusal
would report success. So they gate at the **verb**, before the command reaches us,
with a handler-level gate only as a backstop for routes they cannot read (a `script`
body).

**The question they are actually asking:** is *every* host that wants a failing
effect expected to build a verb interceptor? If yes, that belongs in SPEC §9 as a
stated obligation. If no, it needs a `res_make(0)` path.

**Stance (undecided, leaning toward their proposal).** Let a handler return
`{"ok": false, "error": "…"}` and have the core fail the command with that message.
Backward compatible — no existing handler returns that shape by accident, since
today's convention is "any JSON becomes `data`" — and it makes (a) fixable by the
same test. The reason to review rather than ship: it puts a *schema* on the effect
handler's return value where there was none, and the effect seam is the
[holiday](/concepts/holiday.md) boundary, so a constraint there propagates to every
host. Worth being sure the shape is the one we want before it is load-bearing.

**Related, and part of why this deserves review together rather than piecemeal:**
0.2.7 made a *dispatch* fail loudly on a malformed argument (§6.1 rule 5). The same
argument — a silent `ok:true` is the worst failure shape — applies here with more
force, because an effect touches the world.

## 2. Is the session boundary deliberate? (their item 2)

Measured by Maiz, and entirely consistent with SPEC as written — they were explicit
that this is **not a bug report**:

- `config set actor` works exactly as §9 says; within a session `history` shows
  `[who]` per frame and attribution is perfect.
- **Neither the undo stack nor the log survives `export_state` → `Core(json)`.** A
  second session's `history` is empty; `undo` reports "nothing to undo".

§2 describes a state *document*, not a session, so this follows. The ask is that we
**say so**, because it is invisible until the moment it costs a design and then it
is the whole design. Three questions:

1. Is *"history is session-scoped; `_baseline` is model content"* a boundary we want
   stated in SPEC? They offered to draft the paragraph.
2. Is `status`/`diff`/`revert`-against-`_baseline` the blessed way for a host to
   present *"changes made by someone else since you last looked"*? They have built
   as if yes.
3. Would a **persisted** undo stack ever be on the roadmap? *They are not asking for
   one* — they suspect no, and that no is correct.

**Stance.** (1) yes, it should be stated — this is cheap and purely additive. (2)
leaning yes, and their finding is a good argument for it: reviewing an agent's work,
someone wants *"show me everything it did and let me throw it away"*, not
frame-by-frame undo, and `_baseline` gives exactly that. (3) **no**, and their own
reasoning is the reason: a persisted undo stack puts the *schedule* into the
document, which is close to the thing derived ids exist to keep out, and it grows
unboundedly. Note that (2) does not become blessed until §1(a) is settled — if
`_baseline` can move without persistence, then "changes since you last looked" is
built on a signal that can lie.

**One reading to confirm or correct, because other hosts will copy it:** Maiz's
session persists the state document and deliberately does **not** dispatch `save`,
on the grounds that snapshotting `_baseline` would erase the diff and make the
agent's work indistinguishable from the user's own. That is, **writing the state
document is not the `save` verb.** We believe that reading is right; it has never
been written down.

They also persist the log spine themselves, to a file beside the document, in our
line format (`[ISO] LEVEL op (who): message`), reading §9's "the core does no file
I/O" as making that the front-end's job. That reading is correct.

## 3. Should the derived-id hash be normative? (their item 1)

Reported, no action wanted: they were failing conformance case `15-derived-ids` for
three weeks without knowing, because their C++ runner was dying at process start
(`0xc0000139` — Windows searching the executable's own directory before `PATH` and
finding an older MinGW `libstdc++` without the `std::filesystem` symbols). Their
lesson, which is worth keeping: **"a red conformance run that everyone has agreed to
ignore is indistinguishable from no conformance run."**

They now match our BLAKE2b-48 **byte for byte** rather than re-pinning case 15
against themselves, which §6 permits — and argue the latitude buys nothing:

> *"'not normative' only buys an implementation that agrees with itself, and the
> property worth having — the one the whole case exists for — is two implementations
> agreeing."*

**Stance: agree, and this is the cheapest of the four.** The same argument shaped
0.2.7's `codec_test.py`, which cross-checks the C tokenizer against the Python one
for exactly this reason. If nobody can name what the latitude buys, make the hash
normative and say so in §6. The only reason it is parked rather than done is that
making something normative is a one-way door for every existing implementation, and
it should go out with the other §9 decisions rather than alone.

## Status

`planned`. Nothing here blocks Maiz, and nothing here blocks us. The decision order
if these are taken up together: **1(b)** (the misleading one), then **1(a)** (which
1(b) makes testable), then **2** (which depends on 1(a) for its second answer), then
**3** (independent, cheap). Source: `MESSAGE_FOR_VOIDCORE_maiz-headless-history-and-derived-ids-2026-08-18.md`, quoted above at the points where the wording is the argument.
