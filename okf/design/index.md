# Design

Design rationale and research tracks for Void Core — the *thinking documents* behind
the [concepts](/concepts/index.md), absorbed into this bundle (formerly the standalone
`notes/` folder). These are `audience:dev` and honesty-tagged like any concept: some are
settled rationale (`status:current`), some are forward-looking research (`status:planned`).
When a track matures into contract, it lands in `SPEC.md`.

## The railguard (read first)

* [What Void Core is NOT](/design/what-voidcore-is-not.md) - the overlay-not-runtime boundary; the drift test

## Foundations & architecture

* [Interaction nets — theory](/design/interaction-nets-theory.md) - the mathematical foundation of rune / mantle / holiday
* [C core with FFI bindings](/design/c-core-architecture.md) - why one C library + thin bindings; core vs holiday boundary
* [Domains and guarantees](/design/domains-and-guarantees.md) - where Void Core applies; the forge, not the artifact
* [Command architecture](/design/command-architecture.md) - the one dispatcher command surface

## Transformation layers

* [Reduce / Temper / Scry — design](/design/transform-layers.md) - the three layers: names, forks, resolution
* [Transform layers — app-agent handoff](/design/transform-layers-handoff.md) - the contract brief + replies
* [Transform layers — status update](/design/transform-layers-status.md) - running build-out status

## Knowledge & self-description

* [OKF as a core feature — design](/design/okf-design.md) - a mantle IS an OKF bundle; dev + library bundles
* [App manifest — proposal + decision](/design/app-manifest-design.md) - how an app introduces itself

## Research tracks (planned / north-star)

* [Voidscript as a DSL](/design/voidscript-dsl.md) - grammar-safe orchestration language (PEL direction)
* [Context-size optimization](/design/context-optimization.md) - context as a core pillar; RL-ready later
* [Agent tools, memory, extensions](/design/agent-tools-memory.md) - tool structure, memory, sandbox, extensions
* [Concept brainstorm (archive)](/design/concept-brainstorm.md) - archived early vocabulary brainstorming
