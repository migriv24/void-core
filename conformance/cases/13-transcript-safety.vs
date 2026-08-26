# SPEC §6.1 + §8 — a value in a transcript is DATA, never syntax.
#
# Case 12 pins the tokenizer's rules. This case pins the property those rules
# exist to deliver, at the layer where the family actually meets it: a host takes
# somebody else's text, quotes it per §6.1 into a command transcript, and a human
# reviews the transcript before dispatching it. That shape has converged three
# times independently (Void Hormiga's visitor submissions, Void Reyna's harvested
# datasets, Allomone's proposed rules), and the review is only worth anything if
# a value cannot smuggle a command past it.
#
# It could, until 0.2.7, and not for the reason anyone expected. The ARGV
# tokenizer was fine: a newline inside a quoted run has always been ordinary
# content. The hole was that the statement reader, the condition lexer and the
# interpolator each carried their OWN quote tracking, none of which implemented
# §6.1 rule 3 — so a correctly-quoted value containing an apostrophe closed its
# quote in the reader but not in the tokenizer, and everything after the next
# newline ran as commands, with ok:true.
#
# Every assertion below is a value a hostile (or merely ordinary) author can
# write. `canary` is the rune an injection would reach for.

mantle new transcript
rune new text v
rune new text canary
set canary f untouched

# ── a newline inside a value is content, not a statement boundary ──────────────
set v a 'line one
set canary f BREACHED'
assert $(get canary f --json) == untouched

# ── the apostrophe case: \' must not close the statement's quoted run ──────────
# This is the one that shipped broken. The value is spelled exactly as §6.1's
# algorithm (and `quote_arg`) emits it.
set v b 'I don\'t volunteer.
set canary f BREACHED'
assert $(get canary f --json) == untouched

# ...and the whole value was stored, not the part before the newline.
let b = $(get v b --json)
assert $b == 'I don\'t volunteer.
set canary f BREACHED'

# ── `;` and `}` are statement syntax too, and equally must not leak ────────────
set v c 'don\'t; set canary f BREACHED'
assert $(get canary f --json) == untouched
set v d 'don\'t } set canary f BREACHED'
assert $(get canary f --json) == untouched

# ── single quotes suppress expansion (SPEC §8) ────────────────────────────────
# A transcript is built by quoting somebody else's text. If `$` expanded inside a
# quoted run, that text could run a command — a hole the verb-level filter a host
# puts on a submission cannot see, because the verb it reads is the legitimate one.
set v e 'a stranger wrote $(rune rm canary) and ${canary} and $1'
assert $(get canary f --json) == untouched
let e = $(get v e --json)
assert $e == 'a stranger wrote $(rune rm canary) and ${canary} and $1'

# ── an interpolated value is ONE argument, whatever is inside it ───────────────
# `$x` used to be re-tokenized after expansion, so a stored value silently
# became several arguments (or grew a flag, or vanished when empty).
set v spaces 'two words'
let s = $(get v spaces --json)
set v copy $s
assert $(get v copy --json) == 'two words'

set v flagged 'x --json'
let f = $(get v flagged --json)
set v copy2 $f
assert $(get v copy2 --json) == 'x --json'

set v apos 'a\'b'
let g = $(get v apos --json)
set v copy3 $g
assert $(get v copy3 --json) == 'a\'b'

# an empty expansion is an explicit empty argument, not a missing one
set v blank ''
let h = $(get v blank --json)
set v copy4 $h
assert $(get v copy4 --json) == ''

# ── ${var} is an expansion, not a block (SPEC §8) ─────────────────────────────
let name = world
let greeting = ${name}-ok
assert $greeting == world-ok

# ── and nothing above ran a command ───────────────────────────────────────────
assert $(get canary f --json) == untouched

return 13-transcript-safety-ok
