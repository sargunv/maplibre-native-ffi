---
title: Drive the Event Loop
description: Planned guide for pumping runtime work and draining events.
---

This planned language-agnostic guide covers how a host application drives
MapLibre Native FFI's host-pumped runtime model.

Planned coverage:

- calling the runtime pump from the owner thread;
- draining copied runtime events;
- deciding where event handling belongs in a host application's loop;
- code snippets for supported bindings.
