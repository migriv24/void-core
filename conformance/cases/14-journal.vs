# SPEC §6.2 — pure vs effectful, and the command journal.
# The record is off by default, records the CANONICAL line, classifies the
# holiday boundary statically, and never names two commands the same.

mantle new conform
rune new text a

# Off by default: nothing is recorded until a host asks for the record, and a
# command that ran before it was asked for is not retroactively invented.
assert $(journal --json) == []

journal on
rune new text b
tag b +t1

# One entry per successful top-level mutating command; seq is 1-based and dense.
assert $(journal) == "1 pure rune new text b 2 pure tag b +t1"

# A POSIX alias records in canonical form: one change must not record under two
# spellings (`rm b` is `rune rm b`).
journal clear
rm b
assert $(journal) == "3 pure rune rm b"

# A FAILED command records nothing — it changed nothing, and the §9 log is
# already the record of attempts.
journal clear
set nonexistent-rune v x
assert !$?
assert $(journal --json) == []

# undo/redo appear in the record: a history that omits taking a change back
# replays into a state the author never had.
journal clear
rune new text c
undo
assert $(journal) == "4 pure rune new text c 5 pure undo"

# The holiday boundary, classified STATICALLY: `save` is effectful here even
# though no effect handler is registered and nothing left the process. A
# host-dependent answer would let the same command be a recordable change on one
# peer and not on another.
journal clear
save
assert $(journal) == "6 effectful save"

# `journal` itself is neither mutating nor effectful, so reading or toggling the
# record never appears in it.
journal clear
journal
journal off
journal on
assert $(journal --json) == []

# seq is a name: clearing the record must not let a later command reuse one.
rune new text d
assert $(journal) == "7 pure rune new text d"

return 14-journal-ok
