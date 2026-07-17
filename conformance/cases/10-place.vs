# SPEC §3.2/§6/§7 — `place` and the VIEW SLICE: placement is set through the
# dispatcher (on the mutation spine), but is OUTSIDE the undo slice — place
# takes no history frame, and undo/redo never moves a surviving rune.

mantle new conform10
rune new text a
rune new text b

# set + read round-trip; set returns the placement as data
let p = $(place a 10 20 --json)
assert $(place a --json) == $p

# 3D flavor
place b 1 2 3
let q = $(place b --json)
assert $? == 1

# place pushes NO undo frame: undo right after a place pops the last *rune*
# mutation (rune new b), not the placement; a's placement survives untouched
undo
assert $(place a --json) == $p

# ...and the undone snapshot (taken before either place existed) must NOT
# drag a's placement back to null: the view slice is carried over on restore
redo
assert $? == 1
assert $(place a --json) == $p
assert $(place b --json) == $q

# a later real mutation, undone, still leaves placement alone
tag a +kind:node
place a 77 88
let r = $(place a --json)
undo
assert $(place a --json) == $r

# clear returns the rune to unplaced
place a --clear
assert $? == 1

return 10-place-ok
