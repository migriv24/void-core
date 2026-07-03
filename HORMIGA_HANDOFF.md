# Handoff: requests from the Hormiga integration (2026-07-03)

> From the agent working in `Projects/Hormiga` (public repo `migriv24/hormigas`),
> after shipping Phase 1 of the convergence: `hormiga_core/` embeds the C core via
> the Python binding + `voidcore.Dispatcher`, a Void Console REPL lives in the app's
> Developer tab, and the runtime ships **vendored** inside the installer
> (`Hormiga/scripts/vendor_voidcore.py` → `vendor/voidcore/`, verified against the
> frozen PyInstaller exe). Every claim below was tested against the local repo on
> this date. Miguel asked me to relay three requests; I've reviewed each against
> what actually exists first.

---

## 1. Distribution: a way for consumers to get Void Core versions/updates

**Current facts (verified):**
- Local repo: **one commit** (`31dd1c5 Initial commit: Void Core v0.1.0`) with
  **26 dirty/untracked entries** — the recent work (transform layers, effect
  handler, MeshDB verification, OKF engine…) is not committed anywhere.
- Public mirror `github.com/migriv24/void-core`: same single commit. No tags, no
  releases, no CI, version `0.1.0` in `pyproject.toml`/`vc_version` since inception.

**What downstream actually needs** (not a runtime self-updater — apps vendor a
pinned runtime and ship it with their own releases; that model works and is the
right one):
1. **Commit the local work and sync the mirror.** Hormiga's public README and OKF
   bundle now link to void-core; the linked repo missing the dispatcher seam and
   holidays that Hormiga visibly uses is a bad look. ⚠️ Before pushing: sweep for
   secrets/junk (e.g. `holidays/meshdb/.baas-smoke/` server logs & data dir,
   `__pycache__`, anything in `.claude/`) — Hormiga had to burn its entire history
   over one hardcoded secret; don't repeat that here.
2. **Version discipline:** bump `0.1.0` when behavior changes (one number, three
   places to keep equal: `pyproject.toml`, `package.json`, `vc_version()` in C),
   tag releases, keep the OKF `log.md` as the changelog.
3. **GitHub Releases with prebuilt native libs** — `libvoidcore.dll` (win),
   `.dylib` (mac), `.so` (linux), built by CI (cmake+ninja matrix; the core is
   plain C + vendored cJSON, should be portable). **This is the concrete blocker
   it solves:** Hormiga's mac/linux installers currently ship a degraded console
   because only the Windows DLL exists to vendor. Once release artifacts exist,
   Hormiga's `vendor_voidcore.py` grows a `--from-release <tag>` mode and pins.

## 2. POSIX-flavored command surface (agent ergonomics)

Miguel wants the CLI to feel like a Linux shell — `cd` for mantles, `rm` for
runes, etc. — so agents can lean on their terminal priors. Review of what exists:

**Conformance gap first (bug):** SPEC §7 declares aliases
`rm→rune, ?→help, quit→exit, pwd→where, dump→export`, and the JS oracle has them
(`src/dispatch/dispatch.js:65`) — but the **C core has none**. Verified:
`pwd` → `unknown verb: pwd (try 'help')` via the Python binding. Whatever else
happens, the C core should match the spec it claims.

**Then the enhancement.** The mental model maps cleanly: **mantle ≈ directory,
rune ≈ file, tags ≈ globs**. Suggested desugarings (argument-aware rewrites to
canonical verbs — one semantics, many spellings; aliases must never fork behavior):

| POSIX | canonical |
|---|---|
| `cd <mantle>` | `use <mantle>` |
| `pwd` | `where` |
| `rm <ref>` | `rune rm <ref>` (note: the current spec'd alias `rm→rune` makes `rm x` = `rune x`, which is a usage error — argument-aware is what POSIX hands expect) |
| `mv <a> <b>` | `rune rename <a> <b>` |
| `cp <a> <b>` | `rune dup <a> <b>` |
| `mkdir <name>` | `mantle new <name>` |
| `grep <q>` | `find <q>` |
| `man [verb]` | `help [verb]` |

Two behavior niceties that came up constantly while driving it from Hormiga:
- `ls` with **no active mantle** errors (`no active mantle`). POSIX intuition says
  root-`ls` should list what's there — listing mantles would make cold starts
  self-explanatory for agents.
- `cd` with no args (or `cd /`) could deactivate / go to the mantle list.

Please spec these in §7 (replacing the current alias line), implement in the C
core, and add a conformance case so the JS oracle and C core can't drift.

## 3. Other findings from the integration (in priority order)

1. **Expose the tag-expression evaluator through the C ABI** — e.g.
   `vc_tag_match(expr, tags_json) -> bool` or a batch variant. There are now
   **three** implementations of the SPEC §5 grammar: the C core, Hormiga's
   host-side `hormiga_core/tagexpr.py` (needed to filter *holiday* entities so
   `effect query events "june AND healthcare"` means exactly what `ls --tag`
   means), and `holidays/meshdb/tag_filter.py`. Query-over-holiday is the
   headline workflow — its grammar shouldn't live in per-host copies that can
   drift. One C impl + FFI would let hosts delete theirs.
2. **Voidscript bug, exact repro:** inside `$(…)` capture, `--json` breaks
   `--tag` value parsing:
   ```
   rune alpha tagged month:june
   ls --tag month:june                       → data: ["alpha"]      (direct)
   let n = $(ls --tag month:june); echo $n   → "alpha"              (capture, ok)
   let n = $(ls --tag month:june --json)     → []                   (BUG)
   ```
   Likely the appended `--json` is consumed as/confuses the `--tag` value flag in
   the capture path. Worth a conformance case.
3. **Document the threading contract for `vc_dispatch`.** Hormiga serializes all
   dispatches behind a Python lock as a guess. If the manager is single-threaded
   by design, say so in SPEC §6/§9; if it's reentrant, say that.
4. **Import hygiene / vendorability:** `voidcore/__init__.py` mutates `sys.path`
   and imports generic top-level module names (`temper`, `reduce`, `net`, `lens`,
   `projection`, `roundtrip`, `localjson_holiday`) — collision-prone inside host
   apps, and it forces vendoring to replicate the whole repo layout (my vendor
   script mimics `ROOT`-relative paths to survive). Making scry/temper/reduce
   proper subpackages (`voidcore.scry` …) would fix both, and make a real wheel
   (with the native lib as package data) buildable later. Not urgent — the layout
   is stable and the vendor script copes — but each new consumer pays this tax.

**What Hormiga consumes today** (so you know the load-bearing surface):
`VoidCore(state=…)`, `register_glyph`, `set_effect_handler`, `dispatch`,
`export_state`, `voidcore.Dispatcher` (transform verbs), and these files vendored:
`voidcore/{__init__,dispatch,spec}.py`, `bindings/python/voidcore.py`,
`holidays/localjson/localjson_holiday.py`, `scry/{lens,projection,roundtrip}.py`,
`temper/temper.py`, `reduce/{net,reduce}.py`, `core/build/bin/libvoidcore.dll`.
Changes to any of those ripple into the Hormiga installer at the next
`vendor_voidcore.py` run.

— Hormiga agent, 2026-07-03
