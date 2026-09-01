# Void Core — Architecture

> **Legacy design narrative (JS prototype).** This document describes the original
> JS reference implementation in [`src/`](src/), now kept as the CLI/dispatcher
> **conformance oracle** for the [C core](core/README.md). It predates the C-core
> direction and uses the older "twin manager" framing. For the current, honest
> picture of the system start at the OKF: [`okf/index.md`](okf/index.md) (contract:
> `SPEC.md`; design rationale: [`okf/design/`](okf/design/index.md)).
>
> A CLI, a script runner, the rune/mantle data model, a logging spine, and a tag
> system. No LLM embedded yet — but architected as if one could be dropped in.

---

## 1. What Void Core is (and is not)

**Is:**
- A reusable library + CLI that gives every manager the same skeleton.
- The canonical implementation of **runes**, **mantles**, **domains**, and
  **tags** (defined in Codex §2).
- A **script runner**: the user can run named sequences of commands.
- The **logging spine**: timestamped, levelled, streamed, persisted, copyable.
- The **agent entry point**: the CLI can *describe* a website, not just mutate
  it, so an AI agent can read state and act through one surface.

**Is not:**
- A website builder. That is Hormiga's job (Codex Law 8). Void Core manages
  websites that already exist.
- An LLM host. We leave a clean seam for one (§7) but do not wire it in.
- A per-website thing. It is the abstract parent; managers fork it.

A twin = **Void Core + website-specific runes/editors**.

---

## 2. The data model

### 2.1 Rune

The atomic editable unit. Implemented as a plain serializable object so it can
live in JSON and be diffed in git.

```jsonc
{
  "spirit": {
    "id":   "rune_9fa3c1b7e2",   // real ID: random, stable, never reused
    "name": "nervous-bubble"      // human handle; used by tags and references
  },
  "glyph": "card",                 // the rune's editability type (see 2.2)
  "facets": {                      // free-form metadata for reasoning / LLMs
    "who":   "",
    "what":  "The Nervous System Lab bubble on the hub",
    "when":  "",
    "where": "hub grid, science row",
    "why":   "links outcome science-concepts to its lab site",
    "how":   ""
  },
  "tags": ["science", "outcome:concepts", "linked"],
  "content": { /* glyph-specific editable payload */ },
  "placement": { /* optional explicit position; see Codex §3 */ },
  "relations": [ /* optional layout/relationship edges; see 2.3 */ ]
}
```

- `spirit.id` is minted once and frozen. `spirit.name` is editable but must stay
  unique within its mantle (it doubles as a tag handle).
- `glyph` is what the rune is **edited with** — this is the rune's practical type.
  Void Core ships a small registry of glyphs; twins register more.
- `facets` are the six who/what/when/where/why/how fields. Always present, may
  be empty. They are how the CLI can textually describe *any* rune uniformly.
- `content` is the glyph-specific editable data (text string, image path list,
  dialogue array, character config…). Void Core does not interpret it; the
  rune's **glyph handler** does.

#### 2.2 Rune glyphs (the editability registry)

A **glyph** binds a rune type to (a) how it's edited and (b) how it's described.

```jsonc
{
  "glyph": "text",
  "label": "Text block",
  "editor": "text",              // which GUI editor / CLI prompt to use
  "schema": { "value": "string" },
  "describe": "(rune) => rune.content.value"   // textual summary for CLI/LLM
}
```

Built-in glyphs (minimum): `text`, `richtext`, `image`, `imageList`, `color`,
`link`, `group`. Website-specific glyphs (e.g. `bubble`, `dialogueLine`,
`characterConfig`) are **registered by the twin**, not baked into core.

### 2.3 Mantle

A group of runes over a domain, plus the rules between them (Codex §2, §3).

```jsonc
{
  "id": "mantle_b21f...",
  "name": "biology-hub",
  "domain": "biology-portfolio",     // ref to a domain (2.4)
  "runes": [ /* rune objects, or refs to a rune store */ ],
  "tags":  { /* tag definitions live here; see §3 */ },
  "layout": {                          // (a) relationship graph — Codex §3
    "edges": [ /* { from, to, relation } e.g. {from, to, "below"} */ ]
  },
  "rules": [                           // (b) event/behavior rules — Codex §3
    /* { when: "click:nervous-bubble", then: "navigate:..." } */
  ]
}
```

- **One website can carry multiple mantles.** The biology site is one mantle;
  **Click LaFont** is a second mantle laid on top (Codex §2). Void Core treats
  them as separate mantles over the same domain, stacked.
- `layout.edges` and `rules` are **stored from day one** but in v1 the manager
  may rely on explicit `placement` and rendered code. The solver/rule-engine
  that *consumes* these are future modules (Codex §3). Persisting them now means
  no remodel later.

### 2.4 Domain

The base a mantle sits on — the hosting target.

```jsonc
{
  "name": "biology-portfolio",
  "repo":   "/path/to/some-portfolio-site",
  "liveUrl": "https://example.github.io/some-portfolio-site/",
  "build":  "npm run build",
  "deploy": "npm run deploy",
  "preview": "npm run dev",          // command Void Core runs for localhost
  "port":   4041                      // this manager's local port
}
```

Domains are the seam between the abstract model and the real filesystem/deploy.

---

## 3. Tag system

Tags live **within a mantle** (Codex §2). A tag is just a string handle, but the
system around it is the point:

- Any rune can carry any number of tags.
- A rune's `spirit.name` is itself a usable tag — this is how runes reference
  each other (`tags: ["nervous-bubble"]` means "related to that rune").
- Namespaced tags are encouraged: `outcome:concepts`, `group:science`,
  `status:placeholder`. The CLI filters on these.
- Tags are organizational metadata; they need not reach the front end.

Void Core provides: add/remove tags, list tags with counts, filter runes by tag
expression (`group:science AND NOT status:placeholder`).

---

## 4. CLI surface (Law 9)

The CLI does everything the GUI does **and** can describe the site. Designed so a
human *or* an AI agent can drive it. Verbs are grouped below; every verb is also
an internal dispatcher call (the GUI and the script runner call the same ones).

Global conventions: `--json` on any read verb for machine output; `--help` on any
verb; `<name>` accepts either a rune's `spirit.name` or `spirit.id`; targets can
be a tag expression with `@<expr>` (e.g. `void set @group:science accent "#fff"`).

```
# ── Read / describe (the agent's window; first-class) ───────────────
void describe [<name>]        # textual dump: whole mantle, or one rune's full
                              #   glyph render + who/what/when/where/why/how
void ls [--tag <expr>]        # list runes (filter by tag expression)
void tree                     # mantle -> runes -> relations as an indented tree
void get  <name> [<field>]    # read a content field (or all fields)
void find <query>             # search runes by name/content/facets/tag
void cat  <name>              # raw JSON of a rune
void status                   # what has changed since last save (dirty set)
void diff [<name>]            # diff working state vs last saved
void history [--tail N]       # recent mutations (the undo stack)
void glyphs                   # list registered glyphs and their editors
void mantles                  # list mantles on the current domain
void domain                   # show the current domain (repo, urls, commands)
void validate                 # check the mantle for broken refs / schema errors

# ── Mutate (each pushes onto the undo stack) ────────────────────────
void set    <name> <field> <v>     # edit a content field
void tag    <name> +foo -bar       # add / remove tags
void rune new <glyph> <name>       # mint a rune (auto-generates spirit.id)
void rune rm     <name>
void rune rename <name> <newName>  # rename spirit.name (keeps spirit.id)
void rune dup    <name> [<newName>]
void rune move   <name> <relation> <target>   # set a layout relation edge
void facet  <name> <who|what|...> <v>          # edit one of the six facets
void mantle new <name>             # add a mantle over the current domain
void undo   [N]                    # revert last N mutations
void redo   [N]
void batch  <file>                 # apply a JSON batch of mutations atomically

# ── Lifecycle ───────────────────────────────────────────────────────
void preview start|stop|status     # localhost copy (domain.preview)
void save                          # Save Progress -> write to local site files
void deploy                        # Update Website -> git + build + deploy (streams)
void build                         # build only, no push (domain.build)
void revert                        # discard working changes back to last save

# ── Scripts (§5) ────────────────────────────────────────────────────
void script run  <name> [args...]  # run a saved script
void script ls
void script new  <name>
void script edit <name>
void script show <name>

# ── System / meta ───────────────────────────────────────────────────
void log [--tail N] [--level L]    # print the persisted log
void use <mantle|domain>           # switch the active mantle/domain (cd-like)
void where                         # show active mantle + domain (pwd-like)
void config [get|set <k> [<v>]]    # manager config (ports, paths, defaults)
void export [<file>]               # full mantle export (the "mantle over domain")
void import <file>
void help [<verb>]
void version
void exit | quit                   # leave the interactive REPL

# ── Agent seam (reserved; not wired yet, see §7) ────────────────────
void agent ...
```

Design rules:
- **Read verbs are first-class.** `describe`/`tree`/`status`/`diff` are how an
  agent understands the site; invest in clean, complete output.
- Every command emits structured log lines (§6) and supports `--json`.
- Mutations are **undoable** — they go through one mutation log so `undo`/`redo`
  and `diff` work uniformly.
- The GUI and the script runner call the same internal dispatcher — one core,
  three faces (GUI, CLI, scripts).

---

## 5. Script runner (the Void scripting language)

A *script* is a named, replayable program written in **Voidscript** — a small but
**terminal-complete** language. Every line is either a Void Core command (§4) or a
control construct. The simplest script is just a list of commands; the language
exists so routines can branch, loop, and react to failure. Scripts are stored as
`.void` text files in the manager and run through the same logged dispatcher as
the CLI, so a failed script produces a diagnosable log.

The minimal form is still trivial:

```void
# refresh-and-ship.void
save
preview stop
deploy
```

But the language is expansive enough to be a real terminal. Full surface:

```void
# ── Comments ────────────────────────────────────────────────────────
# everything after # is a comment

# ── Variables & assignment ──────────────────────────────────────────
let site = "biology"            # string / number / bool literals
let n    = 9
set accent = get nervous-bubble accent   # capture a command's output
$site                           # interpolation: $var or ${var}

# ── Command output capture & expansion ──────────────────────────────
let dirty = $(status --json)    # $( ... ) captures command stdout
echo "changed: $dirty"

# ── Conditionals ────────────────────────────────────────────────────
if status --dirty {
  save
} elif validate --quiet {
  echo "clean and valid"
} else {
  echo "nothing to do"
}

# comparison / logic operators: == != < > <= >= && || !
if $n >= 9 && $site == "biology" { echo "ok" }

# ── Loops ───────────────────────────────────────────────────────────
foreach r in (ls --tag group:science) {   # iterate over a query result
  set $r accent "#4d96ff"
}
while status --dirty { save }
repeat 3 { echo "tick" }
# loop control: break / continue

# ── Functions / reusable blocks ─────────────────────────────────────
def ship(msg) {
  save
  deploy --message $msg
}
ship("nightly update")

# ── Error handling ──────────────────────────────────────────────────
try { deploy } catch (e) { echo "deploy failed: $e"; halt 1 }
on error continue        # mode: keep going past failures (default: stop)
on error stop
assert validate --quiet  # abort with message if false

# ── Flow & process control ──────────────────────────────────────────
halt [code]              # stop the script with an exit code
return [value]           # return from a def
wait <ms>                # delay (e.g. let preview boot)
include "common.void"    # run another script inline (import)
call other-script args   # run another saved script as a subroutine
prompt name "Title?"     # ask the user for input (interactive runs only)
echo / print             # write to the log/stdout

# ── Everything in §4 is also a statement ────────────────────────────
# set, tag, rune new, save, deploy, preview, undo, use, export, ...
```

Semantics:
- **Statements** are commands (§4) or control constructs; one per line, `;`
  separates multiple on a line; `{ }` blocks group them.
- **Scope:** `let` is block-scoped; `def` functions take params and `return`.
- **Truthiness:** a command is "true" if it exits 0; `--quiet` suppresses its
  output so it can be used purely as a condition.
- **Failure mode** is configurable (`on error stop|continue`); default is stop,
  so a broken step doesn't silently corrupt a deploy.
- **Args:** `void script run <name> a b c` exposes `$1 $2 $3` and `$@`.

Purpose (Codex spirit): the user codifies routines once, and can copy a script
plus its run-log to hand an agent. Because the language is full (variables,
branches, loops, error handling), an agent can also *write* a Voidscript to
perform a complex edit, rather than issuing commands one at a time.

> Implementation note: Voidscript is an interpreter over the dispatcher, built
> last in the v1 order (§9). Ship the linear-list form first; layer the control
> constructs on once the dispatcher and CLI are solid. The grammar above is the
> target, not a v1-day-one requirement.

---

## 6. Logging spine (Codex §6)

One logger, shared by GUI, CLI, and scripts.

- Format: `[ISO-timestamp] LEVEL op: message` — `LEVEL` ∈ `INFO|WARN|ERROR`.
- **Streams** long ops (deploy, build, preview) line-by-line over SSE to the GUI
  and stdout to the CLI — never just a spinner.
- **Persists** to `logs/void.log` (rotated) so a crash keeps the trail.
- **Copyable**: GUI "Copy log" button; CLI `void log`. The copied text is meant
  to be pasted straight to an AI agent.
- The deploy streaming pattern is already proven in the PortfolioManager's
  `/api/deploy` SSE route — Void Core generalizes it into one `run(cmd)` helper
  that any operation can use.

---

## 7. The LLM seam (architected, not wired)

No LLM is embedded yet. We make it droppable:

- The CLI's `--json` output and the `describe` command form a clean
  **read interface** an agent can consume.
- All mutations go through one **command dispatcher** (the same one the CLI and
  scripts call) — so an LLM "tool layer" would call dispatcher verbs, not poke
  internals.
- `facets` (who/what/when/where/why/how) on every rune exist precisely so an LLM
  has uniform, textual context for any rune.
- Leave a `void agent` namespace reserved in the CLI for when we wire one in.

We do **not** add an LLM dependency, key handling, or network calls now.

---

## 8. Proposed layout

```
VoidCore/
  ARCHITECTURE.md         # this file
  package.json
  src/
    model/                # rune, mantle, domain, spirit (ID minting)
    glyphs/                # built-in rune-glyph registry
    tags/                 # tag store + filter expressions
    log/                  # logging spine + run() streamer
    dispatch/             # the one command dispatcher (CLI + GUI + scripts)
    cli/                  # void <verb> entry point, --json support
    scripts/              # script runner
    server/               # shared Express app: SSE deploy, static GUI host
  gui/                    # shared base GUI (editor shell, log pane, deploy btn)
```

A twin imports `VoidCore`, registers its website-specific **glyphs** and
**editors**, points a **domain** at the real repo, and ships.

**Decision (2026-06-10): shared local dependency, not copy-fork.** Void Core is
one real package living here; each twin depends on it locally (a file/path
dependency) rather than copying the skeleton. Fix the spine once, every twin
inherits the fix. Twins stay disposable at the *content/editor* layer; the spine
is shared. (Revisit only if a twin needs to diverge from the spine itself.)

---

## 9. Build order (matches the user's plan)

1. ✅ Codex (`../TWIN_MANAGER_CODEX.md`) — the laws.
2. ✅ This outline.
3. ✅ **Void Core built**: model → glyphs → tags → log/dispatch → CLI → server/GUI
   shell → Voidscript runner. The `describe`/`set`/`save`/`deploy` loop is
   validated. See `README.md`.
4. ✅ **Biology twin built** on Void Core (`../BiologyManager`, port 4041) — see
   §10. Import → edit → save round-trip verified against `src/data.js` and the
   Click config in `index.html`.

---

## 10. First real twin: the Biology Portfolio manager

Target site: a small Vite + gh-pages portfolio site checked out beside this
repo (its path and live URL are the operator's; nothing here depends on them).

What the user needs it to manage (from the brief — *not* "add more bubbles";
there are exactly **9** concepts, fixed by the assignment):

- **The 9 hub bubbles** — text/caption/title/accent of each `SECTIONS` entry in
  `src/data.js`. → one mantle, nine `bubble` runes.
- **The linked external sites** — the 9 labs (all locally accessible; see the
  hub's `PORTFOLIO-MAP.md` for paths). Edit their text/media through the same
  manager.
- **Click's reactions per site** — what Click LaFont says on each page. →
  Click is its **own mantle** over the same domain; each reaction is a
  `dialogueLine` rune. (Lives in `public/click-overlay.js` `window.CLICK_CONFIG`
  / per-site config.)
- **A mini character engine** — Click's config/sprites as `characterConfig`
  runes (see `../ENGINE.md`). This is a rune glyph, not a container — exactly the
  case Codex §2 calls out.
- **General text/media editing** across the above, like the PortfolioManager.

So the Biology twin = Void Core + glyphs {`bubble`, `dialogueLine`,
`characterConfig`} + two mantles (the hub, and Click on top) over one domain,
with a Save Progress that writes `src/data.js` and the Click configs, and an
Update Website that runs `npm run deploy`. Suggested port: **4041**.
