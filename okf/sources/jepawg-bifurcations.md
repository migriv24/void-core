---
type: Source
title: Göbel & Cheng — Bifurcations from Weight Space: JEPAWG on Parametric Dynamical Systems
description: "Bifurcations from Weight Space: JEPAWG on Parametric Dynamical Systems — cited by the world-models design page."
resource: https://arxiv.org/abs/2610.30847
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Tobias Göbel, Miranda C. N. Cheng, arXiv 2610.30847 (Oct 2026). Builds on JEPAWG (Göbel et
al., arXiv 2607.07127, lattice field theory).

# What it says

Trained networks are treated as **data**. A model zoo is built by fine-tuning a forecaster at
each point of a parameter grid (Lorenz, Rössler, Brusselator, Hénon). JEPAWG learns a joint
latent of weights and parameters with a JEPA objective, then a decoder back to weights, so
weights at unseen parameters can be generated. The norm of the parameter-derivative of the
decoded weights peaks at **bifurcations** (Lorenz Hopf at ρ≈24.5 vs analytic 24.74; the first
five Hénon period doublings). Limits: it sees only what the training trajectories contain, and
cannot resolve features finer than the grid spacing.

# What we use it for

Evidence that **a model is itself an object with a position in a latent space** — the
strongest version of Void Bicho's "a model is a rune". A model zoo indexed by parameters is a
mantle of model runes with measure runes for the parameters; Core needs nothing new to hold
one. See [world models](/design/world-models.md).

# Verify

Read 2026-10-28 from the arXiv HTML through a summarizing fetch.
