# SPEC §3.1 / §3.4 / §4 — identity + reference rules.
# duplicate-name reject, rename keeps content and repoints references,
# taken-name reject, remove drops the rune.

mantle new conform
rune new text one

# duplicate spirit.name rejected within a mantle
rune new text one
assert !$?

# reference by name
set one v hello
assert $(get one v --json) == hello

# rename keeps content; the old name stops resolving
rune new text two
tag two +one
rune rename one uno
assert $(get uno v --json) == hello
get one v
assert !$?

# two's name-tag reference was repointed by the rename
foreach r in (ls --tag uno) { set $r sawuno yes }
assert $(get two sawuno --json) == yes

# rename to a taken name rejected
rune new text taken
rune rename uno taken
assert !$?

# remove: the rune stops resolving
rune rm two
get two sawuno
assert !$?

return 02-identity-ok
