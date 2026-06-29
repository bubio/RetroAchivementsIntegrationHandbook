# rcheevos Emulator Integration Handbook

This repository is not a replacement for
[rcheevos](https://github.com/RetroAchievements/rcheevos). It is a small
handbook and sample set for emulator authors who want to wire rcheevos into any
emulator without treating the emulator core, OS integration, GUI toolkit, or
media format as part of a new SDK.

rcheevos already provides the core RetroAchievements functionality. These notes
focus on the parts rcheevos intentionally leaves to the host application:
lifecycle, HTTP, memory inspection, Hardcore policy, GUI-facing snapshots, and
state-file handling. Concrete emulator sketches live under `examples/`.

Japanese documentation starts at [README.md](README.md).

## Contents

- [docs/en/integration-guide.md](docs/en/integration-guide.md): the main integration
  checklist and architecture notes.
- [docs/en/app-contract.md](docs/en/app-contract.md): the language-neutral contract
  between an RA runtime and the host emulator application.
- [docs/en/gui-model.md](docs/en/gui-model.md): the expected settings, login,
  overlay, achievement, leaderboard, and status UI model.
- [docs/en/credential-storage.md](docs/en/credential-storage.md): cross-platform
  token storage guidance.
- [examples/minimal-c-client](examples/minimal-c-client): a tiny standalone C
  harness that models the host-side lifecycle around `rc_client`.
- [examples/xm8mac-adapter-sketch/README.en.md](examples/xm8mac-adapter-sketch/README.en.md): a
  concrete emulator integration sketch.

## Positioning

Use rcheevos for achievement parsing, game identification, runtime evaluation,
leaderboards, Rich Presence, and RetroAchievements API request generation.

Use these notes for the parts rcheevos intentionally leaves to the emulator:

- when to call frame, idle, load, unload, reset, and progress serialization
- how to route async HTTP completions safely
- how to expose a stable inspection memory map
- how to handle stale callbacks after media changes
- how to structure UI-friendly snapshots and events
- how to persist login tokens without storing passwords

The documents deliberately separate portable contracts from concrete emulator
placement. A different emulator or language binding should start with
`docs/en/app-contract.md` and `docs/en/gui-model.md`, then map those ports onto
its own application structure. Use the examples only as comparison material.
