# SPEC §2, §3.3.3, §6 — glyph declarations live in the state document.
#
# Void Hormiga, 2026-09-03, and the one ask in that message that blocked
# something. Until 0.2.14 a descriptor could only be REGISTERED on the manager,
# by the host, at boot. So a bundle carried its runes but not their meaning: open
# it on a machine whose host registered different descriptors and the content
# survives verbatim in the document while the projection has no fields. The data
# is present and unreachable — which is why an organization's own record type
# could not survive being handed to somebody.
#
# `state.glyphs` is the other half of the document. `scripts`, `domains` and
# `bindings` were already empty top-level keys, so there was both room and
# precedent, and declaring a type is now an ordinary logged, journaled, undoable,
# mergeable command like every other change.
#
# What this case CANNOT show is the travel itself — a case runs inside one
# manager. `bindings/python/glyph_declaration_test.py` exports a document and
# reopens it on a manager that registered nothing, which is the failure shape.

mantle new records

# ── a declaration is a first-class change ───────────────────────────────────
glyph declare '{"glyph":"contact","label":"Contact","fields":["name","email"]}'
assert $? == 1

# the descriptor is a HOST CONTRACT: one shape, defaults resolved, source named
let c = $(glyphs contact --json)
assert $c != null

# runes of a declared glyph are ordinary runes
rune new contact ada
assert $? == 1
let fields = $(get ada --json)
assert $fields != null

# and `validate` accepts them: either registry answers
validate
assert $? == 1

# ── a declaration SHADOWS a host registration of the same name ──────────────
# `text` is a built-in. Declaring it means the document's answer wins, because
# the document's answer is the one that traveled with the data.
glyph declare '{"glyph":"text","label":"Redeclared text","fields":["value"]}'
assert $? == 1

# ...and undeclaring makes the built-in visible again rather than deleting it
glyph undeclare text
assert $? == 1
rune new text plain
assert $? == 1

# ── a declaration is REFUSED rather than half-stored ────────────────────────
# A descriptor outlives the session that wrote it and travels to hosts that never
# saw the code. A typo in one is a fact about a type nobody downstream can
# question, so the shape is checked at the door.
glyph declare '{"label":"no name"}'
assert !$?
glyph declare '{"glyph":"broken","kind":"thing"}'
assert !$?
glyph declare '{"glyph":"broken","fields":["a"],"kinds":{"b":{"level":"ratio"}}}'
assert !$?
glyph declare '{"glyph":"broken","fields":["a"],"kinds":{"a":{"level":"nomnal"}}}'
assert !$?
glyph declare '{"glyph":"broken","presentations":"canvas"}'
assert !$?

# nothing was stored by any of them
glyphs broken
assert !$?

# ── undeclaring is refused while runes still carry it ───────────────────────
# The declaration is what makes its runes readable; removing one under them
# recreates exactly the failure this feature exists to prevent.
glyph undeclare contact
assert !$?
rune rm ada
glyph undeclare contact
assert $? == 1

# an undeclared name is unknown again
rune new contact bob
assert !$?

# ── a declaration UNDOES like a rune, not like config ───────────────────────
# A schema is authored content: it travels with the document and is what makes
# the runes readable, so it belongs in the undoable slice (§6) rather than beside
# `config`, which is a session knob.
glyph declare '{"glyph":"temp","label":"Temporary"}'
assert $? == 1
glyphs temp
assert $? == 1
undo
glyphs temp
assert !$?
redo
glyphs temp
assert $? == 1

return 18-glyph-declarations-ok
