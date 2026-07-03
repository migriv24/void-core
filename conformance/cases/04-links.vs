# SPEC §3.7 — links.
# link/unlink over layout.edges, dangling endpoints are legal,
# repoint-on-rename, drop-on-remove.

mantle new conform
rune new text a
rune new text b

link a b --relation supports
assert $? == 1

# a dangling endpoint is legal (not-yet-created knowledge)
link a ghost
assert $? == 1

# rename repoints edges: the edge to b is now addressed as c
rune rename b c
unlink a c
assert $? == 1

# removing a rune drops its edges: nothing left to unlink
link a c --relation again
rune rm c
unlink a c
assert !$?

return 04-links-ok
