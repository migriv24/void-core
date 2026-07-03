---
type: Concept
title: Domain
description: The base a mantle renders or deploys onto — the seam between the abstract model and the real world.
resource: SPEC.md
tags: [status:current, audience:library, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

A **domain** is what a [mantle](/concepts/mantle.md) sits on: a hosting/output target
(a website repo, an export destination, and — for a consumed
[OKF](/components/okf-engine.md) bundle — the source repo it came from). Its fields
(`repo`, `liveUrl`, `build`, `deploy`, `preview`, `port`) are optional shell commands
or values resolved by a [holiday](/concepts/holiday.md).

The domain is the only place that knows about the real filesystem/network; the rest
of the core stays abstract.

# Status

`current` (the website specialization). Generalizing domains to arbitrary output/
distribution targets is partly `planned`. See `SPEC.md` §3.5. Design: [domains and guarantees](/design/domains-and-guarantees.md).
