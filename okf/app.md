---
type: Manifest
title: Void Core
name: Void Core
id: voidcore
version: 0.2.5
description: A host-agnostic engine other applications build on — runes, mantles, holidays, and the Reduce/Temper/Scry layers.
status: current
authors: [migriv24]
icon: rune
theme: void
palette.primary: "#7c3aed"
palette.accent: "#d946ef"
palette.bg: "#0b0b12"
palette.ink: "#e8e8f0"
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

The **app manifest** for this bundle — Void Core's structured self-description (see
[App manifest](/concepts/app-manifest.md)). A tool that presents Void Core from the outside
(a launcher, a registry, FaultSack — the external OKF study tool) reads this instead of scraping
prose: identity (name / id / version / description) plus an optional representation layer
(palette / icon / theme). The core defines the *shape* and renders nothing.
