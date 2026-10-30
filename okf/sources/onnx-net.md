---
type: Source
title: Qin et al. — ONNX-Net: Towards Universal Representations and Instant Performance Prediction for Neural Architectures
description: "ONNX-Net: Towards Universal Representations and Instant Performance Prediction for Neural Architectures — cited by the world-models design page."
resource: https://arxiv.org/abs/2510.04938
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Shiwen Qin, Alexander Auras, Shay B. Cohen, Elliot J. Crowley, Michael Moeller, Linus
Ericsson, Jovita Lukasik, arXiv 2510.04938 (Oct 2025). Code: github.com/shiwenqin/ONNX-Net.

# What it says

Any network convertible to ONNX is serialized as **text** — chains of
`Op(inputs)(params) --> … --> Output:Shape`, after lossless simplification — and a fine-tuned
**ModernBERT-large encoder** (not a generative LLM) predicts its accuracy. ONNX-Bench holds
649,596 trained architectures. Strong zero-shot transfer between close search spaces, weak
across distant ones (einspace ρ≈0.2). The authors argue Python code is a poor universal
format because one model has many spellings; a canonical graph text has one.

# What we use it for

Two things. **Text is not an LLM-only interface**: a canonical serialization of a graph is
consumed well by a non-generative encoder, so the dispatcher's command language is not itself
the incompatibility. And the canonicality argument is ours — SPEC §6.2's canonical command line
exists for the same reason. See [world models](/design/world-models.md).

# Verify

Read 2026-10-28 from the arXiv HTML through a summarizing fetch.
