# SPEC §7 — the `config` verb (system family): get / set / list, scalar
# coercion (true/false/number/string), and isolation from the undo slice
# (config is host/session meta, not part of the undoable mantles/active state).

# set + get round-trip; coercion: a numeric token becomes a number
config set bpm 140
assert $(config get bpm --json) == 140

# overwrite in place
config set bpm 141
assert $(config get bpm --json) == 141

# strings stay strings
config set title nightdrive
assert $(config get title --json) == nightdrive

# bare `config` lists; ok either way, and data carries the map
config
assert $? == 1

# a missing key reads as ok with empty value (not an error)
config get nosuchkey
assert $? == 1

# config set is NOT undo-tracked: undo rolls back the last *mantle* mutation,
# and the config value written after it survives
mantle new conform
rune new text a
config set bpm 90
undo
assert $(config get bpm --json) == 90

# ...and redo (of the rune mutation) works and leaves config alone
redo
assert $? == 1
assert $(config get bpm --json) == 90

return 09-config-ok
