# SPEC §7.2 — `related` distinguishes "no associations" from "wrong verb",
# and `relate` refuses a weight that is not a number.
#
# Void Hormiga, 2026-09-02. A field agent wrote a link, asked `related` about it,
# and was told "(no neighbors)" about a rune whose edge was sitting in the state
# document. Nothing was broken: `related` is the TAG-proximity verb and reads
# `mantle.tags[<tag>].near`, which only `relate` writes; `links` is the verb that
# reports edges, and it reported this one correctly. But a rune's name doubles as
# a tag, so `related <rune>` is accepted, answers confidently, and the empty
# answer is indistinguishable from "this thing has nothing attached to it". The
# agent concluded `link` had failed and wrote it a second time.
#
# The two verbs model different things and collapsing them would be worse than
# the confusion, so `related` still reports only tag proximity. What changed is
# the EMPTY answer: when the ref names a rune that actually has edges, it names
# the verb that can see them. It never fires on the ordinary tag case.
#
# Found alongside it: `relate a b --relation friend` reached atof("--relation"),
# which is 0.0, and wrote the association with weight ZERO — "not near at all" —
# while returning ok. `relate` takes no flags; a non-numeric weight is now an
# error rather than a silently inverted fact.

# Note for case authors: the empty object is written '{}' here, quoted. Bare {}
# is not a literal in §8 — `}` is a §6.1 statement separator, so it closes the
# block and the comparison loses its right-hand side. `[]` needs no quoting.

mantle new site
rune new text click-dns
rune new text click-pages

# ── the report, exactly as filed ────────────────────────────────────────────
link click-dns click-pages --relation 3:1
assert $? == 1

# `links` was always right
let edges = $(links click-dns --json)
assert $edges != []

# `related` still reports NO tag proximity — that part was never wrong
let near = $(related click-dns --json)
assert $near == '{}'

# ...but the empty answer now points at the verb that can see the edge
related click-dns
assert $? == 1

# ── the signpost must NOT fire on the ordinary cases ────────────────────────
# a tag naming nothing at all
related no-such-thing
assert $? == 1
let none = $(related no-such-thing --json)
assert $none == '{}'

# a rune with no edges: still just "(no neighbors)"
rune new text lonely
related lonely
assert $? == 1

# ── real tag proximity is untouched ─────────────────────────────────────────
relate spring summer 0.5
assert $? == 1
let warm = $(related spring --json)
assert $warm == '{"summer":0.5}'

# a rune name used as a genuine tag still resolves as one
relate click-dns dns-stuff 0.25
let both = $(related click-dns --json)
assert $both == '{"dns-stuff":0.25}'

# ── `relate` refuses a weight that is not a number ──────────────────────────
relate alpha beta --relation friend
assert !$?

# and wrote nothing: the association must not exist at weight 0
let alpha = $(related alpha --json)
assert $alpha == '{}'

# a plain typo is caught by the same rule
relate alpha beta abc
assert !$?

# valid weights still work, including negative and exponent forms
relate alpha beta 2.5
assert $? == 1
let a2 = $(related alpha --json)
assert $a2 == '{"beta":2.5}'

relate gamma delta -1.5e-2
assert $? == 1

return 17-related-signpost-ok
