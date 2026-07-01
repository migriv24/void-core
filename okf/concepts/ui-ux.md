---
type: Concept
title: UI / UX
description: An abstract core assertion — every Void Core app has a user who experiences it, so an app must describe its UI and UX, even though the core defines no widgets and renders nothing.
tags: [status:planned, audience:library, audience:dev, confidence:exploratory]
timestamp: 2026-07-01T00:00:00Z
---

Void Core renders **nothing** — it defines the *shape* of an application (runes,
mantles, domains, the [dispatcher](/concepts/dispatcher.md)) and leaves presentation
to the host (see [what Void Core is NOT](/design/what-voidcore-is-not.md)). But every
application has a **user**, and that user *experiences* the app and *interacts* with
it somehow. So UI/UX is a first-class **abstract** aspect of Void Core, even without a
single widget in the core.

The assertion is deliberately minimal and host-neutral:

> Any Void Core application **must describe its UI and UX** — how a user perceives its
> state and how they act on it — as part of introducing itself.

# What this is (and is not)

- **Is:** a required *description*. Every app has a user surface; the app should say
  what it is (a CLI, a web panel, a physical device) and how a user drives it. This
  pairs naturally with the [app manifest](/concepts/app-manifest.md) (identity +
  representation): the manifest's `palette`/`icon`/`theme` are the *representation*
  vocabulary a renderer can read, and rendering itself belongs to a renderer holiday.
- **Is not:** any core widget toolkit, template, or QT/HTML component. The core ships
  no visual and mandates no framework.

# Deliberately abstract — CLI to IoT

Every app's UI/UX differs, and the core must accommodate all of them without favoring
one. The [CLI](/concepts/dispatcher.md) is the baseline user surface; a web panel is
another; and a longer-term target is **embedded / IoT** hosts (ESP32, Arduino) where
the "UI" may be a few buttons and an LED. Keeping this concept abstract is what lets
the same rune/mantle model sit under a terminal, a browser, and a microcontroller.

# Status

`planned` — an abstract principle today (the core already renders nothing and pushes
presentation to the host). Formalizing *how* an app declares its UI/UX (a manifest
field? a required design page?) is open; a concrete **renderer/representation
holiday** that consumes the [app manifest](/concepts/app-manifest.md) is the likely
first built piece.
