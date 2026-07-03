# SPEC §6/§7/§8 — regression (Hormiga handoff 2026-07-03): a trailing flag after
# a `--tag <expr>` value must not join into the tag expression. The headline
# repro was `let n = $(ls --tag month:june --json)` returning [] because the
# appended --json was consumed as part of the expression.

mantle new conform8
rune new text alpha
tag alpha +month:june

# direct: same command, with and without a trailing --json, must agree
let plain = $(ls --tag month:june --json)
assert $plain == ["alpha"]

# capture without --json takes the lines (text) path
let t = $(ls --tag month:june)
assert $t == alpha

# quoted expression with a trailing flag
let q = $(ls --tag "month:june" --json)
assert $q == ["alpha"]

# multi-word unquoted expression still joins up to (not past) the next flag
rune new text beta
tag beta +month:june +kind:event
let m = $(ls --tag month:june AND kind:event --json)
assert $m == ["beta"]

# and inside foreach, the same command drives iteration
foreach r in (ls --tag month:june --json) { set $r seen yes }
assert $(get alpha seen --json) == yes
assert $(get beta seen --json) == yes

return 08-capture-flags-ok
