---
type: Source
title: LeCun — A Path Towards Autonomous Machine Intelligence
description: "A Path Towards Autonomous Machine Intelligence — cited by the world-models design page."
resource: https://openreview.net/pdf?id=BZ5a1r-kVsf
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Yann LeCun, position paper v0.9.2 (2022-06-27). The origin of the JEPA programme.

# What it says

Six modules: **configurator** (sets up the others for the task), **perception** (estimates the
world state), **world model** (predicts future states under imagined action sequences, and
fills in what perception missed), **cost** (an immutable intrinsic cost plus a trainable
critic, as one scalar "energy"), **short-term memory** (current and predicted states with
their costs), **actor** (proposes action sequences, minimizes predicted cost). Mode-1 is
reactive; Mode-2 plans through the world model, MPC-style. JEPA is the proposed world-model
architecture: predict in representation space, not pixels; H-JEPA stacks levels at different
timescales.

# What we use it for

The module map Void Core's parts are laid against in [world models](/design/world-models.md).

# Verify

Recalled and cross-checked against secondary summaries 2026-10-28; the paper itself was not
re-read.
