# SPEC §6 / §7 — batch atomicity.
# a failing command rolls the whole batch back; a good batch applies
# atomically as ONE undo frame.

mantle new conform
rune new text a
set a v before

# rollback on any failure
batch '["set a v mid", "bogus-verb x"]'
assert !$?
assert $(get a v --json) == before

# atomic apply, one undo frame for the whole batch
batch '["set a v one", "tag a +t1"]'
assert $? == 1
assert $(get a v --json) == one
let tagged = none
foreach r in (ls --tag t1) { let tagged = $r }
assert $tagged == a

undo
assert $(get a v --json) == before
let after = none
foreach r in (ls --tag t1) { let after = $r }
assert $after == none

return 06-batch-ok
