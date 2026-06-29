# Application Contract

This document defines the host-side contract that an emulator application must
provide around rcheevos. It is intentionally language-neutral. C++, Swift, C#,
Rust, Kotlin, or F# integrations should be able to map these ports and models to
their native idioms.

The names below are descriptive, not mandatory API names.

## Runtime Boundary

The RA runtime owns:

- rcheevos `rc_client` state
- login and game-load state
- generation id for async invalidation
- achievement, leaderboard, Rich Presence, and user snapshots
- pending rcheevos operations
- event normalization

The application owns:

- UI rendering
- HTTP implementation
- credential persistence
- media selection and execution-media policy
- emulator memory inspection
- main-thread dispatch
- image fetching/decoding if the UI displays remote badges
- state-file placement and emulator core state serialization
- policy enforcement before mutating emulator state

Do not let UI code hold rcheevos pointers. Convert every rcheevos string, list,
and event to application-owned data before exposing it outside the RA runtime.

## Required Host Ports

| Port | Implemented by app | Called by RA runtime | Notes |
|---|---|---|---|
| `HttpTransport` | Yes | Yes | Sends RA API requests and returns status/body asynchronously. |
| `CredentialStore` | Yes | Yes | Saves, loads, and deletes login tokens. Passwords are never persisted. |
| `MainThreadDispatcher` | Yes | Yes | Moves HTTP completions and UI events back to the thread that owns `rc_client` and app state. |
| `MemoryInspector` | Emulator core | Yes | Side-effect-free address reads for `rc_client_do_frame` and Rich Presence. |
| `MediaProvider` | App/media library | Yes | Provides media bytes/paths, hash input, execution media, and media-change metadata. |
| `SessionPolicy` | App | App and UI | Allows or denies reset, pause, load state, full speed, media changes, and debug tools. |
| `StateStore` | App | App | Wraps emulator state with RA progress metadata when RA mode permits state save/load. |
| `UiPresenter` | App | Runtime emits data | Draws settings, status, achievements, leaderboards, notifications, and errors. |
| `ImageProvider` | App | UI | Fetches and caches badges, game images, and avatars. May reuse `HttpTransport` with image-specific rules. |
| `Logger` | App | Runtime | Logs sanitized diagnostics only. |

Only `MemoryInspector` needs to touch emulator internals. HTTP, credentials,
SQLite, badge decoding, and UI should stay outside the emulation core.

## Core Data Model

Use host-owned immutable snapshots when possible.

```text
RaSnapshot
  status
  mode
  generation
  hardcore_enabled
  user?
  game?
  achievements[]
  leaderboards[]
  rich_presence?
  offline_reason?
  disconnected?
```

```text
RaUser
  username
  display_name
  score
  softcore_score
```

```text
RaGame
  id
  title
  hash
  image_url
```

```text
RaAchievement
  id
  title
  description
  points
  bucket
  bucket_label
  state
  unlocked
  measured_progress
  measured_percent
  rarity
  image_url
```

```text
RaLeaderboard
  id
  title
  description
  bucket
  bucket_label
  state
  tracker_value
  format
  lower_is_better
  top_entries[]
```

Keep raw rcheevos numeric event ids available for debugging, but expose
application-level event names to UI.

## Runtime Events

The runtime should publish these host-owned events:

- `LoginStateChanged`
- `GameIdentified`
- `GameLoadFailed`
- `AchievementTriggered`
- `AchievementProgress`
- `AchievementChallengeShown`
- `AchievementChallengeHidden`
- `LeaderboardStarted`
- `LeaderboardTrackerUpdated`
- `LeaderboardFailed`
- `LeaderboardSubmitted`
- `RichPresenceChanged`
- `Disconnected`
- `Reconnected`
- `ServerError`
- `ResetRequested`
- `OfflineSessionStarted`
- `PolicyDenied`

Each event should include `generation` and `game_id` when a game is active. UI
must ignore events whose generation no longer matches the current snapshot.

## Threading Rules

- Use one owner thread for `rc_client` and RA runtime state.
- HTTP workers must not mutate emulator, UI, or `rc_client` state directly.
- HTTP completions are queued through `MainThreadDispatcher`.
- Every async request captures the current generation.
- Increment generation on login reset, game load, unload, logout, and destroy.
- Completion callbacks for stale generations must not update application state.
- Shutdown order is: stop frame calls, cancel host HTTP, abort rcheevos async
  operations, drain/settle callbacks, destroy `rc_client`.

## Memory Inspection Contract

`MemoryInspector.read(address, size)` must:

- return bytes without modifying emulator state
- be valid only for documented RA address ranges
- clamp or fail out-of-range reads consistently
- avoid CPU wait insertion, I/O acknowledgement, or bank side effects
- present banked/mirrored memory in a stable RA-facing map
- stay stable during one frame evaluation

Returning fewer bytes should be treated as a failed read by the RA runtime. A
small page cache inside the runtime or bridge is acceptable when repeated
rcheevos reads are expensive.

## Media Contract

For file-backed media, removable media, cartridge images, disc images, or
multi-media launch sets, the application should provide:

- stable media id or content hash
- original path or display name for UI only
- bytes/path used for rcheevos identification
- execution media path/handle when RA mode requires a separate writable copy
- slot, drive, bank, disc, or launch metadata for emulator insertion

## State Contract

`StateStore` should keep RA progress separate from normal emulator state policy.

Required behavior:

- ask `SessionPolicy` before save/load
- serialize rcheevos progress with sized APIs
- record game id, media hash, rcheevos version, progress size, and checksum
- validate all metadata before applying emulator core state
- apply RA progress only after the emulator state is accepted
- reject old or foreign states in online RA sessions

If the emulator already has a state-file format, extend it with an explicit RA
progress section rather than mixing RA progress into the opaque emulator core
blob. Record version, size, and checksum for that section.

## Language Mapping

The contract maps cleanly to common languages:

| Concept | C/C++ | Swift | C# / F# | Rust |
|---|---|---|---|---|
| Runtime handle | class or opaque pointer | final class | class implementing `IDisposable` | struct with explicit lifetime |
| Host ports | abstract class/function table | protocols | interfaces/delegates | traits |
| Events | queue of structs | enum with associated values | discriminated union/events | enum |
| Snapshot | value struct | struct | record | struct |
| Async HTTP | worker + completion queue | `URLSession` + main actor handoff | `HttpClient` + synchronization context | async task + channel |
| Memory read | callback | closure | delegate | closure/trait method |

If a language cannot safely expose rcheevos directly, add a thin C ABI bridge
for that language. Keep the host contract unchanged so behavior stays portable.
