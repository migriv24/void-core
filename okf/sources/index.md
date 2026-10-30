# Sources

External works this bundle leans on — one page per work, carrying what it is, what *we*
use it for, why it is credible, and **what a verification pass should check**.

The convention (adopted from Void Maiz, 2026-08-07, and described in
[OKF spec](/references/okf-spec.md)): a page cites a source with an **ordinary body link**,
never a frontmatter key, because `bundle.py` builds the graph from body links. So
`okf get sources/<name>` lists every page that cites it under `linked from` — and **that
list is the blast radius**: if the source is wrong, it names exactly which pages inherit
the error. A bibliography tells you what was cited; a graph tells you what breaks.

Every page here is **`confidence:asserted`** — written from a model's recall, *not* checked
against the artifact. That is the honest state and it is meant to be visible:
`okf query "confidence:asserted source"` is the standing verification queue. A folder that
called its own recollections `verified` would be worse than no folder at all.

* [Lafont — Interaction Nets / Interaction Combinators](/sources/lafont-interaction-nets.md) - the foundation of Reduce; the only one promoted into a normative contract others implement against
* [Ousterhout — A Philosophy of Software Design](/sources/ousterhout-philosophy-of-software-design.md) - where "deep module" comes from; was cited nowhere until this folder existed
* [WCAG — contrast thresholds](/sources/wcag-contrast.md) - the numbers `theme.py` implements from scratch; the only mechanically checkable source here
* [Google Cloud — Open Knowledge Format](/sources/google-open-knowledge-format.md) - the format this bundle claims conformance to, and its reference implementation
* [Pocock — Skills For Real Engineers](/sources/pocock-skills-for-real-engineers.md) - the MIT-licensed skills adapted into `.claude/skills/`; carries a licence obligation
* [You et al. — A control theory of predictability](/sources/control-theory-predictability.md) - plan-cost on reachable states, not prediction error, governs control; why an exact oracle matters
* [Göbel & Cheng — JEPAWG bifurcations](/sources/jepawg-bifurcations.md) - models as objects in a latent space
* [Resani & Shi — Inside-JEPA](/sources/inside-jepa.md) - relations ground through inverse dynamics; a mantle is that dataset
* [Qin et al. — ONNX-Net](/sources/onnx-net.md) - canonical graph text as a universal representation, read by a non-generative encoder
* [Zhang et al. — Frozen in a Frame](/sources/frozen-in-a-frame.md) - a snapshot carries configuration, not rate
* [LeCun — A Path Towards Autonomous Machine Intelligence](/sources/lecun-autonomous-machine-intelligence.md) - the six-module architecture JEPA sits in
* [Assran et al. — V-JEPA 2](/sources/v-jepa-2.md) - the MIT-licensed video encoder and action-conditioned world model
* [LeJEPA and LeWorldModel](/sources/lejepa-and-leworldmodel.md) - SIGReg, linear identifiability, a 15M world model trainable on one GPU
* [ARC-Bench](/sources/arc-bench.md) - closed-loop success hides broken action ranking
* [Slot-JEPA and Causal-JEPA](/sources/slot-jepa.md) - object identity across time is the hard part; runes get it free
