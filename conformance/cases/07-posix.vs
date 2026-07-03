# SPEC §7.1 — the POSIX surface: aliases are argument-aware desugarings
# (one semantics, many spellings), plus the cold-start behaviors:
# root-ls lists mantles, `use` with no args (or /) deactivates.

# cold start: no active mantle -> ls lists mantles (empty), and does not error
let root = $(ls --json)
assert $root == []

# mkdir -> mantle new (creates + activates)
mkdir conform7
assert $? == 1
rune new text alpha
set alpha v one
tag alpha +k:a

# pwd -> where
pwd
assert $? == 1

# grep -> find
let g = $(grep alpha --json)
assert $g == ["alpha"]

# man -> help
man ls
assert $? == 1

# cp -> rune dup (content copied, fresh identity)
cp alpha beta
assert $(get beta v --json) == one

# mv -> rune rename
mv beta gamma
assert $(get gamma v --json) == one
get beta v
assert !$?

# rm -> rune rm (argument-aware: `rm x` = `rune rm x`, NOT `rune x`)
rm gamma
get gamma v
assert !$?

# aliases don't fork undo semantics: rm pushed a frame, undo restores gamma
undo
assert $(get gamma v --json) == one

# cd / -> use / -> deactivate; root-ls now lists the mantle
cd /
let m = $(ls --json)
assert $m == ["conform7"]

# cd <mantle> -> use <mantle>: back inside, runes visible again
# (gamma was dup'd from alpha, so it carries k:a too, and undo restored it)
cd conform7
let r = $(ls --tag k:a --json)
assert $r == ["alpha","gamma"]

return 07-posix-ok
