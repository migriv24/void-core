# SPEC §3.7/§7.2 — `validate` classifies an unresolved link endpoint.
#
# A link endpoint that does not name a rune in this mantle has two very different
# causes, and until 0.2.10 `validate` reported both as "dangling" with equal
# confidence. A DANGLE is legitimate — links tolerate not-yet-created knowledge, and
# a host streaming chunks in and out has edges dangle constantly with nothing wrong.
# An endpoint that names a MANTLE is a mistake: v1 links are rune<->rune within one
# mantle, so the name resolves to the wrong KIND of thing. A host that cannot tell
# them apart has to treat the case it was told to ignore as a possible typo forever.
# (Void Unity, 2026-08-28, boxing an equipment mantle into a world mantle.)
#
# The problem strings for EDGES are part of the contract, which is why this case
# asserts on `data` and not only on `ok`. Each expectation is wrapped in single
# quotes with `\'` for the apostrophes it contains — §6.1's own escape, since the
# strings being pinned quote the offending name.

mantle new world
rune new text player
rune new text wand

# both endpoints resolve here: no problem at all
link player wand --relation holds
validate
assert $? == 1
let clean = $(validate --json)
assert $clean == []

# an endpoint naming nothing is a DANGLE: legal, and still reported
link player ghost
validate
assert !$?
let dangle = $(validate --json)
assert $dangle == '["dangling edge to \'ghost\'"]'

unlink player ghost

# an endpoint naming a MANTLE is cross-kind, and says so instead
mantle new equipment
use world
link player equipment --relation carries
validate
assert !$?
let cross = $(validate --json)
assert $cross == '["cross-kind edge to \'equipment\': names a mantle, not a rune"]'

# the two coexist, each keeping its own wording
link player ghost
let both = $(validate --json)
assert $both == '["cross-kind edge to \'equipment\': names a mantle, not a rune","dangling edge to \'ghost\'"]'

# the `from` side classifies exactly like the `to` side
unlink player ghost
unlink player equipment
link equipment wand --relation carried-by
let fromside = $(validate --json)
assert $fromside == '["cross-kind edge from \'equipment\': names a mantle, not a rune"]'

# a mantle that goes away takes its cross-kind reading with it: the SAME edge reads
# as a plain dangle once nothing answers to the name. The classification is a fact
# about the state document, not a flag stored on the link.
mantle rm equipment
use world
let after = $(validate --json)
assert $after == '["dangling edge from \'equipment\'"]'

return 16-validate-endpoints-ok
