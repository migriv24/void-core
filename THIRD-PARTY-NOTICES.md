# Third-party notices

Void Core itself is MIT (see [`LICENSE`](LICENSE)). It vendors exactly one
third-party component, and depends on nothing from npm or PyPI.

## cJSON

- **Files:** `core/vendor/cJSON.c`, `core/vendor/cJSON.h`
- **Upstream:** https://github.com/DaveGamble/cJSON
- **Copyright:** (c) 2009-2017 Dave Gamble and cJSON contributors
- **License:** MIT

Vendored rather than fetched so a clean clone builds with nothing but CMake and
a C compiler. The full license text is reproduced verbatim at the top of both
files. MIT imposes no condition on downstream users beyond keeping that notice
with the source, which vendoring the headers satisfies.

## Project skills (`.claude/skills/`)

- **Files:** `.claude/skills/**`
- **Upstream:** https://github.com/mattpocock/skills
- **Copyright:** Matt Pocock, "Skills For Real Engineers"
- **License:** MIT

Adapted, not verbatim: pointers to a `CONTEXT.md` were broadened to also read
this project's OKF bundle. `.claude/skills/README.md` records which skills were
installed and which were deliberately left out. These are authoring aids for
contributors; nothing in the library reads them.

## Nothing else

- **Python:** the binding, the holidays and the transformation layers use only
  the standard library. `holidays/meshdb` names `neo4j` as an optional import,
  loaded lazily, and is not required to build, install or test Void Core.
- **JavaScript:** the `src/` conformance oracle requires only Node built-ins
  (`fs`, `path`, `crypto`, `events`, `readline`, `child_process`). There is no
  `package-lock.json` because there are no dependencies to lock.
