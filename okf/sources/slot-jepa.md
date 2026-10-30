---
type: Source
title: Slot-JEPA and Causal-JEPA — object-structured latent world models
description: "object-structured latent world models — cited by the world-models design page."
resource: https://arxiv.org/abs/2610.28074
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Two works on the same point. **Slot-JEPA** (arXiv 2610.28074, Oct 2026): dense patch
latents are a redundant **prediction interface** — every downstream predictor must re-derive
object structure, and without **explicit entity correspondence across time** history is costly
to use; stable semantic slots plus time-varying masks expose a reusable object-structured
interface. **Causal-JEPA** (Nam, Le Lidec, Maes, LeCun, Balestriero, arXiv 2602.11389, ICML
2026; code github.com/galilai-group/cjepa): mask an object's history except an identity
anchor so its state must be inferred from the *other* objects; about 20% better counterfactual VQA,
planning with 1% of a patch model's latent features.

# What we use it for

A rune is a slot whose identity across time is **guaranteed rather than learned** — `spirit.id`
is minted once and never content-derived (SPEC §3.1). These papers spend their effort
recovering exactly that. See [world models](/design/world-models.md).

# Verify

Abstracts read 2026-10-28; Causal-JEPA's licence not checked.
