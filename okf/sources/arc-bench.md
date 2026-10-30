---
type: Source
title: Zhang & Li — ARC-Bench: Closed-Loop Replanning Masks Broken Action Ranking in Frozen JEPA World Models
description: "ARC-Bench: Closed-Loop Replanning Masks Broken Action Ranking in Frozen JEPA World Models — cited by the world-models design page."
resource: https://arxiv.org/abs/2609.05461
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Zhengshu Zhang, Zhiyuan Li, arXiv 2609.05461 (Aug 2026).

# What it says

Reward-free latent planners assume that **closeness to the goal embedding ranks actions the
way true cost does**. A fixed-candidate, no-leak protocol on official JEPA-WM checkpoints
shows the top-scored candidate is almost always suboptimal, with or without V-JEPA encoders.
Frequent replanning hides it: forcing longer open-loop commitment drops success from 89.6% to
61.8% on PointMaze and from 36.1% to 12.5% on Push-T.

# What we use it for

The evaluation discipline: a world model attached to a Void host is judged by **fixed-candidate
ranking against the true outcome**, which the core can compute exactly for pure commands. A
closed-loop success rate is not evidence. See [world models](/design/world-models.md).

# Verify

Abstract read 2026-10-28.
