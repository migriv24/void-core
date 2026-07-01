# Void Core

A host-agnostic engine other applications build on: a CLI, a Voidscript runner, the
rune/mantle/domain data model, a logging spine, and a tag system. No LLM embedded
yet — architected so one can be dropped in (see `ARCHITECTURE.md` §7).

**Standalone project.** Its own git repo; applications depend on it locally (via a
`file:`/editable install) and are developed separately — Void Core stays isolated.

> **Agents & contributors: start at [`okf/index.md`](okf/index.md)** — the
> [Open Knowledge Format](okf/references/okf-spec.md) bundle that describes Void
> Core itself (concepts, components, design rationale, roadmap), with `status:`
> honesty on what is built vs planned. It is the self-describing map of the project.

Documents, by role:
- **[`okf/`](okf/index.md)** — the knowledge bundle. Start here to understand the system.
- **`SPEC.md`** — the normative, language-agnostic contract (what any implementation
  must do); indexed from the OKF at [`okf/references/spec.md`](okf/references/spec.md).
- `ARCHITECTURE.md` — the JS prototype's design narrative (the conformance oracle).
- The C core lives in [`core/`](core/README.md); design rationale is in
  [`okf/design/`](okf/design/index.md) (the absorbed research notes).

## Install

```bash
npm install
```

## Use it as a CLI (standalone, any state file)

```bash
node src/cli/cli.js --state ./demo/state.json            # interactive REPL
node src/cli/cli.js --state ./demo/state.json describe   # one-shot
node src/cli/cli.js --state ./demo/state.json --json ls  # machine output
```

## Use it as a library (what an app does)

```js
const { createManager } = require('void-core');

const manager = createManager({
  stateFile: __dirname + '/state.json',
  domains: [{ name: 'my-site', repo: '...', deploy: 'npm run deploy', port: 4042 }],
  glyphs:  [ /* app-specific glyphs */ ],
  adapters: { save: async (ctx) => { /* write runes back to real files */ } },
  bootstrap: ({ state, logger }) => { /* import the live data into runes once */ },
});

// Drive the manager's dispatcher directly; the app owns its own UI.
manager.dispatch('describe');
```

An app = **Void Core + domain-specific glyphs + a save adapter**. Void Core
renders nothing — the host owns its UI (see [ui-ux](okf/concepts/ui-ux.md)).

## The model in one breath

- **rune** — atomic editable unit: a `spirit` (frozen real-ID + human name), six
  `facets` (who/what/when/where/why/how), a `glyph` (how it's edited), `content`,
  `tags`.
- **mantle** — a group of runes over a domain, plus a layout graph + rule set.
- **domain** — the real hosting target (repo, build/deploy/preview commands, port).
- **tag** — organizational metadata; a rune's name doubles as a tag.

## Command surface

`describe ls tree get find cat status diff history glyphs mantles domain validate
where · set facet tag rune mantle undo redo batch · preview save deploy build
revert · script · log use config export import help version exit`

Run `help` in the REPL, or read `ARCHITECTURE.md` §4. The scripting language
(Voidscript) is `ARCHITECTURE.md` §5.

## Layout

```
src/model/      rune, mantle, domain, spirit, store
src/glyphs/     glyph registry + built-ins (text, image, color, link, …)
src/tags/       tag store + filter-expression parser (AND/OR/NOT)
src/log/        logging spine + run() process streamer
src/dispatch/   the one command dispatcher (CLI + scripts call it)
src/scripts/    Voidscript interpreter
src/cli/        CLI + REPL
```
