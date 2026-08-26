# SPEC §6.1 — argument quoting.
# The tokenizer strips bare quotes, honors exactly one escape (\' inside single
# quotes), and treats an unterminated quote as end-of-argument rather than an error.
# Getting this wrong is SILENT and corrupts content rather than structure: three
# independent codebases shipped the same mistake (reported by Void Hormiga,
# 2026-08-17). Every value below is one a real host tried to store.
#
# Two authoring notes:
#  - `assert` needs a captured value bound with `let` before a multi-word right-hand
#    side (see 05-voidscript.vs).
#  - Voidscript's EXPRESSION lexer strips quotes but has no \' escape, unlike the
#    argv tokenizer this case pins. So a backslash-bearing expectation is pinned via
#    `setjson` — a different, well-defined encoding path — rather than as a literal.

mantle new quoting
rune new text v

# ── rule 1: whitespace separates; single quotes hold a value together ──────────
set v spaced 'a b c'
let a = $(get v spaced --json)
assert $a == 'a b c'

# ── rule 3: \' is a literal apostrophe inside single quotes ────────────────────
# The Hormiga bug: emitting \'' (backslash AND a closing quote) closed the argument
# early, so an Allomone comment reading "don't" truncated the rest of the script.
set v apos 'don\'t'
let b = $(get v apos --json)
assert $b == 'don\'t'

set v apos2 'it\'s a test'
let c = $(get v apos2 --json)
assert $c == 'it\'s a test'

# ── rule 4: double quotes have no escape, so a " must ride inside single quotes ─
set v dq 'say "hi" loudly'
let d = $(get v dq --json)
assert $d == 'say "hi" loudly'

# ── rule 3, second half: every OTHER backslash is literal ─────────────────────
# JSON payloads and text escape codes must survive untouched.
set v esc 'code \cY and \n stay literal'
let e = $(get v esc --json)
assert $e == 'code \cY and \n stay literal'

# ── rule 2: quoting is strip-anywhere, not delimiting ─────────────────────────
set v joined a'b'c
let f = $(get v joined --json)
assert $f == abc

# ── the trailing-backslash trap (§6.1) ────────────────────────────────────────
# Expected value pinned through setjson: JSON "C:\\" decodes to C:\ .
setjson v want '"C:\\"'

# The CORRECT spelling closes the quote before the trailing backslash, which is
# what `quote_arg` emits: 'C:' followed by a bare backslash.
set v path 'C:'\
let g = $(get v path --json)
let w = $(get v want --json)
assert $g == $w

# ── rule 5, changed in 0.2.7: an unterminated quote is an ERROR ──────────────
# It used to run to end of input and return ok:true, which §6.1 itself named as
# the reason every bug in this class is silent — the naive four-line helper turns
# `C:\` into an argument that never closes, and everything after it (the rest of
# the line, or in a transcript the rest of the file) is swallowed into the value.
# Demonstrated through `batch`, which gives each command its own line boundary so
# this case can carry on afterwards; the whole batch rolls back on the failure.
batch '["set v unterm ok", "set v bad \"oops"]'
assert !$?

# ...and neither command was applied, so nothing was silently half-written.
set v unterm untouched
assert $(get v unterm --json) == untouched

# ── an empty argument is expressible ──────────────────────────────────────────
set v empty ''
let i = $(get v empty --json)
assert $i == ''

return 12-arg-quoting-ok
