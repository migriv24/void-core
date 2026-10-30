---
type: Source
title: Zhang et al. — Frozen in a Frame: The Velocity Blind Spot in JEPA World Models
description: "Frozen in a Frame: The Velocity Blind Spot in JEPA World Models — cited by the world-models design page."
resource: https://arxiv.org/abs/2610.04585
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Tinghe Zhang, Chunyu Liu, … Alex Lamb, arXiv 2610.04585 (Oct 2026). Code:
github.com/OliverZ-dot/TI-JEPA; checkpoints on Hugging Face (TingheOliver/TI-JEPA-checkpoints).

# What it says

A single rendered frame (no motion blur) is a function of **configuration alone**, so a
single-frame JEPA target **cannot identify velocity** for any encoder; the L²-optimal predictor
averages over it, and a static goal image cannot ask to arrive slowly. On the released LeWM
checkpoints every linear velocity probe is at chance while position probes reach R²≈0.95.
The fix, **TI-JEPA**, splits the latent into a pose code and an explicit finite-difference
**motion code** predicted jointly. RateIdent is a three-part diagnostic (probe ladder, a
v/−v "kill" test, inertial planning).

# What we use it for

The state document is a snapshot. It carries configuration and no rate, so a model trained on
exported states alone inherits exactly this blind spot. Rate lives in **history** (the journal)
or in **explicit quantities** (measure runes) — and an act rune is a reified change, which is
the motion code's job done by hand. See [world models](/design/world-models.md).

# Verify

Read 2026-10-28 from the arXiv HTML through a summarizing fetch.
