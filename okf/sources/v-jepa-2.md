---
type: Source
title: Assran et al. — V-JEPA 2: Self-Supervised Video Models Enable Understanding, Prediction and Planning
description: "V-JEPA 2: Self-Supervised Video Models Enable Understanding, Prediction and Planning — cited by the world-models design page."
resource: https://arxiv.org/abs/2506.09985
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-10-28T00:00:00Z
---

# What it is

Meta FAIR, arXiv 2506.09985 (June 2025). Code github.com/facebookresearch/vjepa2;
checkpoints in the Hugging Face `facebook` V-JEPA 2 collection and in `transformers`
(`VJEPA2Model`). **MIT** (a few dataset utilities Apache-2.0).

# What it says

A video encoder pretrained by latent prediction on internet-scale video (ViT-L 300M to ViT-g
1B), strong on motion understanding and action anticipation. **V-JEPA 2-AC** post-trains a
300M action-conditioned predictor on under 62 h of unlabeled DROID robot video and plans
pick-and-place zero-shot toward image goals. Known limits: camera-position sensitivity,
long-horizon error accumulation, image (not language) goals. V-JEPA 2.1 (2026) adds dense
features.

# What we use it for

The open-weight, permissively licensed perception encoder a Void host can actually run
(ViT-L fits a 6 GB GPU for inference). See [world models](/design/world-models.md).

# Verify

Licence checked 2026-10-28 against the repo README and model cards via search; confirm per
checkpoint before shipping (one third-party source lists the giant encoders as Apache-2.0).
