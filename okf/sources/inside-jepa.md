---
type: Source
title: Resani & Shi — Inside-JEPA: From Prediction to Relational Grounding in Latent World Models
description: "Inside-JEPA: From Prediction to Relational Grounding in Latent World Models — cited by the world-models design page."
resource: https://arxiv.org/abs/2610.31194
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Hossein Resani, Javen Qinfeng Shi, arXiv 2610.31194 (Oct 2026).

# What it says

Places **relational channels for every ordered pair of entities** (K soft symbols in (0,1))
inside the JEPA latent state, forecast by the predictor alongside per-entity latents. An
**inverse-dynamics loss** — recover the action from the channels before and after it —
grounds the channels without relation labels; latent prediction alone does **no better than a
random relation head** on real video. Relations (left_of, above, overlap, inside, near, …) are
computed from 2D boxes for evaluation only. Negative results are many: label-free objectives,
shuffled labels and 1–5% labels fail to ground; with a frozen V-JEPA 2 encoder the gain falls
to +.016.

# What we use it for

A mantle already is what this paper has to approximate: ordered rune pairs with **typed
relation labels** (`layout.edges`, `i:j` ports), and an **action label for every change**
(the journal). Inverse dynamics is free when every transition carries the command that caused
it. See [world models](/design/world-models.md).

# Verify

Read 2026-10-28 from the arXiv HTML through a summarizing fetch.
