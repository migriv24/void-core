---
type: Source
title: You et al. — A Control Theory of Predictability in Latent World Models
description: "A Control Theory of Predictability in Latent World Models — cited by the world-models design page."
resource: https://arxiv.org/abs/2607.10362
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Hanzhe You, Yonggang Zhang, … Yike Guo, arXiv 2607.10362 (July 2026). A Koopman-operator
and control-theoretic analysis of JEPA-style latent world models used for model-predictive control.

# What it says

- A planner's suboptimality is bounded by **twice the gap between predicted and true plan
  cost on the candidate set** (`D(û) − D(u*) ≤ 2·sup|D̂ − D|`), and the bound is attained.
  The data-averaged prediction error neither bounds nor tracks it.
- The planner queries **states its candidate actions reach**, which leave the training
  manifold. There a model can have **holes** — false minima where a bad action is predicted
  cheap. Only underestimates matter, because the planner takes the minimum.
- Holes close with **counterfactual data** in the affected region or an **off-manifold
  penalty** on the planning cost — not with a lower average residual.
- Measured: rank correlation with control success was 0.024 for validation loss and 0.62 for
  rollout fidelity on the planner-reachable set. A linear state-readout loss helped convergence.

# What we use it for

The argument that Void Core's most valuable contribution to a world model is an **exact
oracle on planner-reachable states**: for pure commands the core *is* the true dynamics, so
counterfactual data costs a fork, not an experiment. See [world models](/design/world-models.md).

# Verify

Read 2026-10-28 from the arXiv HTML through a summarizing fetch, not the full text. Check the
theorem statement and the reachable-set definition against the paper before quoting numbers.
