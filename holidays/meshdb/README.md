# MeshDB holiday — a local offline graph BaaS for Void Core

A Void Core **holiday** (SPEC §10.1) backed by [MeshDB](https://github.com/mesh-db/meshdb),
a Cypher-compliant graph database, run in **single-node mode** as a local, offline
Backend-as-a-Service. This is the *default data holiday*: a graph store, not SQLite,
because in a graph DB a rune's **tags are edges** — so the tag membership the whole
core is built on (SPEC §5) becomes a native graph query, and the weighted tag-graph
has somewhere real to live.

## The key architectural point

**A MeshDB holiday is a Bolt/Cypher *client*, not a linked dependency.** All the Rust
lives inside the `meshdb-server` process; this holiday spawns/attaches to that server
and talks to it over `bolt://`. Nothing in Void Core's C core is rewritten to use it.
"MeshDB is Rust" does **not** imply "Void Core must be Rust" — the holiday boundary is
exactly the thing that makes the backend's language irrelevant.

## Files

| file | what |
|---|---|
| `meshdb_holiday.py` | the holiday: local-BaaS lifecycle + the holiday interface (insert/get/query/update/delete/describe) over Void Core runes |
| `tag_filter.py` | compiles a Void Core tag-filter expression (SPEC §5) to a Cypher predicate |
| `smoke.py` | end-to-end test: build runes in the C core → sync to the holiday → assert `holiday.query(expr) == core ls --tag expr` |

## Graph model

```
(:Rune {id, name, glyph, content, f_who, f_what, f_when, f_where, f_why, f_how})
(:Mantle {name})
(:Tag {name})
(r:Rune)-[:IN_MANTLE]->(m:Mantle)
(r:Rune)-[:TAGGED]->(t:Tag)
```

`content` is opaque to the core, so it crosses as a JSON string. A rune's own `name`
and `glyph:<name>` count as tags (SPEC §5), so the filter translator tests those
alongside real `:Tag` edges.

## Prerequisites

1. **Rust toolchain** + **LLVM/libclang** (RocksDB's bindgen needs `libclang.dll`;
   set `LIBCLANG_PATH` to the LLVM `bin` dir when building).
2. Build the server (in the meshdb checkout): `cargo build -p meshdb-server`
3. `pip install neo4j`
4. Build the Void Core DLL (for the smoke test): `cmake --build core/build`

## Use

```python
from meshdb_holiday import MeshDBHoliday

# stand up a private local BaaS (spawns meshdb-server, own data dir, no auth)
holiday = MeshDBHoliday.local_baas(root_dir="C:/.../hormiga-baas")

holiday.insert(rune, mantle="content")          # materialize a Void Core rune
holiday.query("@month:june AND type:event")     # resolve a tag expression -> [rune]
holiday.get("intro")                            # by name or spirit.id
holiday.describe()                              # capabilities / status / counts
holiday.close()                                 # closes driver + stops the server

# or attach to a server someone already started:
MeshDBHoliday.connect("bolt://127.0.0.1:7687")
```

`local_baas(reuse=True)` is idempotent: if `:7687` is already serving it attaches
instead of spawning a second server, so it's safe to call on every app start.

## Run the smoke test

```bash
python holidays/meshdb/smoke.py
```

## Why this matters for Hormiga

Hormiga is Python and needs an offline-first data backend. This holiday gives it a
real local graph BaaS today (no cloud, no Supabase), reachable from the same Python
process, with a clean path to the distributed MeshDB modes (routing / Raft /
multi-raft) later — without changing the holiday's interface or the core.
