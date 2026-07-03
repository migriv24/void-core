# SPEC §8 — the Voidscript core subset.
# let + $var/${var} interpolation, if/elif/else, while, repeat,
# foreach + break/continue, $?, and the §6 unknown-verb error contract.

# variables + interpolation
let x = world
let msg = hello-$x
assert $msg == hello-world
let braced = ${x}!
assert $braced == world!

# if / elif / else
let mode = b
let branch = none
if $mode == a { let branch = A } elif $mode == b { let branch = B } else { let branch = C }
assert $branch == B

# while (reassignment ends it)
let s = go
while $s == go { let s = stop }
assert $s == stop

# repeat
let r = none
repeat 3 { let r = again-$r }
assert $r == again-again-again-none

# foreach iterates a command's data array
mantle new conform
rune new text r1
rune new text r2
rune new text r3
let last = none
foreach n in (ls) { let last = $n }
assert $last == r3

# break
let first = none
foreach n in (ls) { let first = $n; break }
assert $first == r1

# continue
let seen = x
foreach n in (ls) {
  if $n == r2 { continue }
  let seen = $seen-$n
}
assert $seen == x-r1-r3

# $? reflects the last command's ok; an unknown verb fails without throwing
ls
assert $? == 1
bogus-verb something
assert !$?

return 05-voidscript-ok
