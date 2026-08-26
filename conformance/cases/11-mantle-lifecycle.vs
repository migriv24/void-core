# SPEC §3.4/§7.2 — `mantle rm` / `mantle rename`: the mantle-level analogues of
# `rune rm` / `rune rename`. Both mutate the undoable slice (mantles + active),
# so both take a normal undo frame. `rm` of the ACTIVE mantle deactivates (the
# `use` / `cd /` cold-start semantics) instead of refusing; `rename` of the
# active mantle carries `active` along.

mantle new conform11a
rune new text a
set a v one

mantle new conform11b
rune new text b

let both = $(mantles --json)
assert $both == ["conform11a","conform11b"]

# rename the ACTIVE mantle: active follows the new name (we stay inside it)
mantle rename conform11b renamed11
assert $? == 1
let after = $(mantles --json)
assert $after == ["conform11a","renamed11"]
let inside = $(ls --json)
assert $inside == ["b"]

# rename is undoable
undo
let back = $(mantles --json)
assert $back == ["conform11a","conform11b"]

# a taken name is refused, like `mantle new`
mantle rename conform11b conform11a
assert !$?

# unknown mantles are refused (and leave no history behind)
mantle rename nope other
assert !$?
mantle rm nope
assert !$?

# rm the ACTIVE mantle -> deactivates; root-ls then lists what is left
mantle rm conform11b
assert $? == 1
let left = $(mantles --json)
assert $left == ["conform11a"]
let root = $(ls --json)
assert $root == ["conform11a"]

# rm is undoable: the mantle, its runes, AND the active pointer come back
undo
let restored = $(mantles --json)
assert $restored == ["conform11a","conform11b"]
let runes = $(ls --json)
assert $runes == ["b"]

# rm of a NON-active mantle leaves the active one alone
mantle rm conform11a
assert $? == 1
let one = $(mantles --json)
assert $one == ["conform11b"]
let still = $(ls --json)
assert $still == ["b"]

# the name is free again after rm (the "emptied but undeletable" rough edge)
mantle new conform11a
assert $? == 1

# rmdir -> mantle rm (the POSIX pair for mkdir; mantle ≈ directory)
rmdir conform11a
assert $? == 1
let final = $(mantles --json)
assert $final == ["conform11b"]

return 11-mantle-lifecycle-ok
