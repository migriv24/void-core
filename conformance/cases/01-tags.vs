# SPEC §5 — tag matching + the filter grammar.
# name-as-tag, glyph:<name>, AND/OR/NOT keywords (case-insensitive), the symbolic
# forms && / || / !, parentheses, implicit AND, and @<expr> multi-targeting.

mantle new conform
rune new text alpha
rune new text beta
rune new group gamma
tag alpha +group:science +status:draft
tag beta +group:science +status:published

# a rune is matched by its own name
foreach r in (ls --tag alpha) { set $r byname yes }
assert $(get alpha byname --json) == yes
get beta byname
assert !$?

# glyph:<name> counts as a tag
foreach r in (ls --tag glyph:group) { set $r isgroup yes }
assert $(get gamma isgroup --json) == yes
get alpha isgroup
assert !$?

# AND + NOT, lower-case keywords (operators are case-insensitive)
foreach r in (ls --tag "group:science and not status:draft") { set $r pub yes }
assert $(get beta pub --json) == yes
get alpha pub
assert !$?

# OR + parentheses + the symbolic forms
foreach r in (ls --tag "(status:draft || glyph:group) && !beta") { set $r hit yes }
assert $(get alpha hit --json) == yes
assert $(get gamma hit --json) == yes
get beta hit
assert !$?

# implicit AND (adjacency)
foreach r in (ls --tag "group:science status:draft") { set $r imp yes }
assert $(get alpha imp --json) == yes
get beta imp
assert !$?

# @<expr> selects many runes for one mutating verb
set @group:science bulk yes
assert $(get alpha bulk --json) == yes
assert $(get beta bulk --json) == yes
get gamma bulk
assert !$?

# a lone or mid-word `&`/`|` is a tag character, not an operator (oracle
# tokenization) — must not crash, and the malformed tag simply matches nothing
foreach r in (ls --tag "alpha&glyph:group") { set $r amp yes }
get alpha amp
assert !$?
foreach r in (ls --tag "alpha & beta") { set $r amp2 yes }
get alpha amp2
assert !$?
foreach r in (ls --tag "alpha | beta") { set $r pipe yes }
get alpha pipe
assert !$?

return 01-tags-ok
