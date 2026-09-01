# SPEC §8 — a CR is a line terminator, not content.
#
# This case is stored with CRLF ON PURPOSE (see ../../.gitattributes, which marks
# it `-text` so git never normalizes it). Every other case is LF. Void Unity
# reported the reason on 2026-08-27: a CR used to be an ordinary character to the
# Voidscript statement reader, so a CRLF-authored script compared wrong values
# and RETURNED wrong ones, with no error on either path —
#
#     return ok<CR><LF>       ->  data was "ok<CR>"   (almost the right string)
#     assert a == a<CR><LF>   ->  false               (the CR was in the token)
#     assert 1 == 1<CR><LF>   ->  true                (numeric coercion ate it)
#
# The third line is what made it expensive: the same script passed or failed
# depending on which KIND of value a line happened to compare, so it read as an
# intermittent logic bug rather than a newline problem.
#
# It surfaced on a Windows host, where CRLF is simply what a text editor, a Unity
# TextAsset or a designer's clipboard hands you — none of them a text mode the
# host controls. And this suite could not have caught it: run.py read cases in
# Python text mode, whose universal-newline translation rewrote CRLF to LF before
# the library saw a byte. The runner now reads bytes, which is what makes this
# file a test rather than a second copy of case 05.
#
# The rule: outside a quoted run a CR ends the statement exactly as LF does
# (skip_sep already treated it as a separator; the statement reader did not, and
# that disagreement WAS the bug — the §6.1 lesson in a different costume).
# Inside a quoted run a CR is data, like any other byte.

mantle new crlf
rune new text r

# 1. a value does not carry the CR of the line it was written on
set r v plain
assert $(get r v --json) == plain

# 2. string comparison — the case that used to be false
let s = abc
assert $s == abc

# 3. numeric comparison — the case that used to pass, hiding the other two
let n = 7
assert $n == 7

# 4. a line ending inside a quoted run is still content, so this is ONE
#    statement and its value carries the break rather than ending at it
set r w 'a
b'
assert $? == 1
assert $(get r w --json) != a
assert $(get r w --json) != ab

# 5. control flow reads its condition off the same statement text
if $s == abc {
  set r flag yes
}
assert $(get r flag --json) == yes

# 6. `return` is where the corruption crossed the C ABI — a host keying on the
#    result got a string that was almost, but not, the one the author wrote.
return 15-crlf-ok
