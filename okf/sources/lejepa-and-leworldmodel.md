---
type: Source
title: Balestriero, LeCun et al. — LeJEPA and LeWorldModel
description: "LeJEPA and LeWorldModel — cited by the world-models design page."
resource: https://arxiv.org/abs/2603.19312
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Two works, one recipe. **LeJEPA** (Balestriero & LeCun, arXiv 2511.08544, Nov 2025; code
github.com/rbalestr-lab/lejepa): the isotropic Gaussian is the optimal embedding distribution,
enforced by **SIGReg** (random 1D projections plus an Epps–Pulley normality test; linear cost; one
hyperparameter; no stop-gradient, EMA or teacher). **LeWorldModel** (Maes, Le Lidec, Scieur,
LeCun, Balestriero, arXiv 2603.19312, Mar 2026): the first JEPA world model trained stably
end-to-end **from pixels** with two loss terms; ViT-tiny encoder plus a 6-layer AdaLN predictor,
about 15–18M parameters, planning by CEM. Trained and evaluated through **stable-worldmodel**
(github.com/galilai-group/stable-worldmodel, **MIT**). The theory follow-up "When Does LeJEPA
Learn a World Model?" (arXiv 2605.26379) proves **linear identifiability** of latents under
stationary additive-noise transitions, uniquely for Gaussian latents.

# What we use it for

The smallest credible world model a Void host could **train itself** on one GPU in hours, on
transitions the core records. And the identifiability result is why a *linear* readout from a
latent to a rune's typed fields is a principled check. See [world models](/design/world-models.md).

# Verify

Abstracts and secondary sources read 2026-10-28. The LeWM checkpoint licence was not found;
check before shipping weights.
