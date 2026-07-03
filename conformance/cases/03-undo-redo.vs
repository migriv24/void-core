# SPEC §6 — mutation invariants.
# every mutating verb pushes an undo frame; redo walks forward; a fresh mutation
# clears the redo stack; mutations mark the state dirty.

mantle new conform
rune new text a
set a v first
set a v second
assert $(get a v --json) == second

undo
assert $(get a v --json) == first

redo
assert $(get a v --json) == second

# a new mutation clears the redo stack
undo
set a v third
redo
assert !$?
assert $(get a v --json) == third

# mutations mark the working state dirty
status --dirty
assert $? == 1

return 03-undo-redo-ok
