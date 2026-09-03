# SPEC §3.3.1, §3.3.2, §3.7.1 — three rune kinds, and a weight that is a value.
#
# Void Hormiga, 2026-09-03, relaying the author's design intent. Not every rune is
# the same KIND of thing, and the differences are structural rather than
# domain-specific: an ENTITY is an explicit thing (a contact, a player, an enemy);
# an ACT is a change (`flying`, `across`) that modifies how entities relate; a
# MEASURE is a dimension something has an amount of (`speed`, `health`).
#
# "Superman flying across the sky" is two entities, an act, and a domain the act
# ranges over — and an edge label can only ever express a BINARY relation, so
# there is no way to write it as one labelled edge without losing a participant.
# The standard move is to reify the verb as a node with typed ports, which is
# RDF reification, neo-Davidsonian event semantics, and — precisely — an
# interaction-net agent. An act rune is not a new mechanism; it is the mechanism
# this stack already has, applied to verbs instead of only to nouns.
#
# The second half: when an edge points AT a measure rune, its weight IS the value
# of that attribute, and the measure rune supplies the unit. That is optional and
# is chosen per application. The rule: if a number is read by RULES that produce
# new structure it belongs on an edge where the rules can see it; if it is read
# only by renderers it belongs in a field. Fields are not deprecated by any of
# this and never will be.

mantle new world

# ── the three kinds, declared ───────────────────────────────────────────────
glyph declare '{"glyph":"being","label":"Being","kind":"entity","fields":["sprite"]}'
glyph declare '{"glyph":"motion","label":"Motion","kind":"act","fields":["manner"]}'
glyph declare '{"glyph":"stat","label":"Stat","kind":"measure","fields":["note"]}'
assert $? == 1

# a glyph that states no kind is an ENTITY, which is what every rune that existed
# before this feature already was — nothing migrates
glyph declare '{"glyph":"plain","label":"Plain"}'
let plain = $(glyphs plain --json)
assert $plain != null
rune new plain nothing-special
assert $? == 1

# an unknown kind is refused rather than stored
glyph declare '{"glyph":"nope","kind":"gamma"}'
assert !$?

# ── entities, an act, and the ternary sentence ──────────────────────────────
rune new being superman
rune new being sky
rune new motion flying
assert $? == 1

# The act is a rune, so the sentence keeps all three participants. As one
# labelled edge it could only ever have kept two.
link superman flying --relation agent
link flying sky --relation path
assert $? == 1
let e = $(links flying --json)
assert $e != []

# kinds are queryable without being tags: `kind:` is already an ordinary app
# namespace on the `what` axis (§5), so reserving it would have changed what
# every existing `kind:vegetable` tag matches
let acts = $(ls --kind act --json)
assert $acts == ["flying"]

# ── a measure rune carries the QUANTITY, and its unit ───────────────────────
# The unit belongs to the rune, not to its glyph: `speed`, `health` and
# `strength` share one schema and differ only in what they measure.
rune new stat speed
rune new stat health
measure speed --level ratio --unit m/s --min 0
assert $? == 1
measure health --level ratio --unit hp
assert $? == 1

# a bare `measure <ref>` reads
let q = $(measure speed --json)
assert $q != null

# the annotation is refused on a rune that is not a measure — the dimension is
# what has a unit, not the thing that has an amount of it
measure superman --unit kg
assert !$?

# and its shape is checked, like a glyph's
measure speed --level fortnight
assert !$?
measure speed --min zero
assert !$?

# ── the weight IS the value ─────────────────────────────────────────────────
link superman speed --weight 900
link superman health --weight 100
assert $? == 1

let v = $(values superman --json)
assert $v != []

# "the fastest thing in this mantle" is structural rather than a field scan
rune new being wolf
link wolf speed --weight 9.5
let fast = $(values --measure speed --json)
assert $fast != []

# an edge to a non-measure rune is NOT an attribute assertion — recognition, not
# coercion. `superman -agent-> flying` is a weighted edge and stays one.
let none = $(values flying --json)
assert $none == []

# ── a weight that is a value must never be a silent zero ────────────────────
# `--weight` used to reach atof(), which answers 0.0 for a flag, a typo and a
# bare word alike. That recorded a wrong-but-plausible edge strength before; it
# would now record the false claim that something's speed is zero.
link wolf speed --weight fast
assert !$?
let still = $(values wolf --json)
assert $still != []

# ── fields keep working, and are still the right answer for most numbers ────
# A grid column and a date are read by renderers and by nothing else. Nothing
# here makes them second-class.
set wolf note '9.5 by default'
assert $? == 1
let note = $(get wolf note --json)
assert $note == '9.5 by default'

return 19-kinds-and-values-ok
