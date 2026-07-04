# RetroAchievements Integration Guide

This guide describes how to integrate rcheevos `rc_client` into an emulator
without depending on a specific machine, media format, GUI toolkit, OS API, or
implementation language.

Each emulator project should keep its own requirements and directory layout as
the source of truth. This guide captures the common decisions most integrations
need: lifecycle, HTTP handoff, memory inspection, Hardcore policy, state
handling, and UI-facing data. Concrete adapter sketches live under `examples/`.

## Recommended Shape

Use three small layers:

1. **rcheevos layer**
   - Owns `rc_client_t`.
   - Calls `rc_client_begin_login_with_password`,
     `rc_client_begin_login_with_token`,
     `rc_client_begin_identify_and_load_game`, `rc_client_do_frame`,
     `rc_client_idle`, `rc_client_unload_game`, `rc_client_reset`,
     `rc_client_serialize_progress_sized`, and
     `rc_client_deserialize_progress_sized`.
   - Converts rcheevos lists and events into host-owned values before returning
     them to the application.

2. **host runtime layer**
   - Owns login/load status, generation id, pending HTTP requests, current
     emulator session pointer, Hardcore preference, Rich Presence cache, and
     UI snapshots.
   - Exposes methods such as `login`, `load_game`, `unload_game`,
     `process_frame`, `pump`, `reset`, `serialize_progress`, and
     `deserialize_progress`.

3. **emulator adapter layer**
   - Provides a side-effect-free inspection memory reader.
   - Calls the runtime from media load/unload, frame execution, pause, reset,
     state save/load, and settings changes.
   - Draws UI using snapshots and events. The integration layer should not own
     emulator UI.

The language-neutral host responsibilities are defined in
[app-contract.md](app-contract.md). The expected GUI surfaces are defined in
[gui-model.md](gui-model.md). Use this document for lifecycle and rcheevos
integration rules, not as the complete UI specification.

Avoid adding a separate wrapper dependency until the host integration actually
needs one. `rc_client` already provides the core API; the value here is
lifecycle discipline and testable host behavior.

## Lifecycle

Use an explicit state machine:

- `LoggedOut`
- `Authenticating`
- `Ready`
- `LoadingGame`
- `Active`
- `OfflineSession`

The emulator must keep running if login or game identification fails. Treat the
media as an offline session until the user reloads it or retries RA.

Typical flow:

1. Create `rc_client_t` with memory and server callbacks.
2. Set the event handler and log handler.
3. Apply the persisted Hardcore preference.
4. Login with password or stored token.
5. On successful login, store only the token, not the password.
6. On media load, increment a generation id, set `LoadingGame`, attach the
   current emulator session, reapply Hardcore, and call identify/load.
7. On successful game load, enumerate achievements and leaderboards, fetch
   leaderboard context if desired, and enter `Active`.
8. On each emulated frame, set the current session, call `rc_client_do_frame`,
   refresh dirty lists after the frame call returns, then clear the current
   session.
9. When paused, call `rc_client_idle` periodically, for example once per second.
10. On unload/logout/dispose, increment generation, cancel host HTTP work,
    abort pending rcheevos requests, clear game lists and Rich Presence, then
    publish a fresh snapshot.

## HTTP Handoff

rcheevos generates API requests but does not perform HTTP itself. The server
callback should:

- allocate or assign a request id
- capture the current generation id
- dispatch GET when `post_data` is null, otherwise POST
- preserve `content_type`, status code, and response body
- enforce a response size limit, such as 8 MiB
- convert timeout/cancellation to a client error status
- never log passwords, tokens, POST bodies, or full authenticated URLs
- marshal completion back to the emulator/main thread if `rc_client` is not
  being used from the HTTP worker thread
- drop the completion if its generation id no longer matches

The generation check is important. Without it, a slow login, game load, or
leaderboard request can complete after the user has unloaded the media and mutate
the next session.

## Memory Reader

Expose an RA inspection memory map, not live CPU bus side effects.

The memory callback should be:

- side-effect-free
- stable during one `rc_client_do_frame` call
- bounded by a documented address range
- independent of current CPU wait states or device timing
- responsible for bank/mirror normalization before returning bytes

If repeated rcheevos reads are expensive, use a small cache in the runtime or
bridge and invalidate it before each frame and before Rich Presence evaluation.

Document the RA-facing address map for the emulator. If RA logic needs banked
RAM, VRAM, cartridge RAM, save RAM, device memory, or work RAM that is not in
the normal CPU linear range, map those regions into a stable inspection range
and keep the banking policy on the emulator side. The core-facing addition
should be a small side-effect-free inspection API, not general RA knowledge
inside the emulator core.

## Events and Snapshots

Do not let UI code query rcheevos structures directly. Convert events and lists
into host-owned values:

- user
- game
- achievements
- leaderboards
- Rich Presence
- Hardcore enabled
- status
- generation id

When rcheevos raises an event during `rc_client_do_frame`, mark dirty flags and
refresh lists after `rc_client_do_frame` returns. This avoids re-enumerating
achievement or leaderboard lists while rcheevos is still processing a frame.

Recommended dirty rules:

- achievement unlock or challenge show/hide: refresh achievements
- leaderboard start/update/cancel/submit: refresh leaderboards
- achievement unlock: refresh user score
- progress indicator-only events: emit an event but do not refresh the whole
  achievement list

Rich Presence should be sampled periodically, not every frame. One second is a
reasonable default. Clear it immediately when game or login state ends.

The application should render these values through its own UI layer. The same
snapshot/event model can map to SDL overlays, native windows, mobile views, web
views, CLI logs, or tests.

## Hardcore and Controlled Operations

All emulator operations that can affect achievement integrity should pass
through one policy function before mutating the emulator state.

At minimum, route these operations through the policy:

- pause
- reset
- save state
- load state
- game change
- cheats
- rewind
- slow motion
- frame advance
- input playback
- debugger access

Recommended defaults:

- Softcore allows normal state operations.
- Hardcore denies load state.
- Save state creation can remain allowed, but loading it cannot.
- Pause during an active RA session should call `rc_client_can_pause` and report
  the remaining frames/time when denied.
- Fast-forward is not necessarily restricted, but keep it explicit in the
  policy so the decision is visible.

If enabling Hardcore triggers a reset event, reset the emulator and acknowledge
the reset with `rc_client_reset`.

## State Files

Save normal emulator state separately from RA progress. For RA-aware state
restore, persist an envelope with:

- magic and version
- game id
- media hash
- rcheevos version
- emulator core state
- sized `rc_client` progress
- checksum

Before loading, validate game id, media hash, rcheevos version, sizes, and
checksum. If validation fails, do not apply the RA progress. The emulator may
still choose to load normal state depending on its Hardcore policy.

Keep a hard maximum for serialized RA progress. The exact limit is host policy;
16 MiB is a practical starting point for a defensive upper bound.

## Credentials

Store only tokens. Do not persist passwords.

Recommended service/account shape:

- service: reverse-DNS app id, for example `org.example.Emulator.RetroAchievements`
- account: RA username
- secret: RA token

Use OS credential stores where available:

- macOS: Keychain generic password with
  `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly`; use the Data Protection
  keychain (`kSecUseDataProtectionKeychain`) so the token can be read after the
  first unlock without prompting on every access.
- Windows: Credential Manager, Generic Credential
- Linux desktop: Secret Service via libsecret

If secure storage is unavailable, prefer not persisting the token over silently
writing secrets to a plaintext config file. If an emulator intentionally
supports a file fallback for portable builds, require restrictive permissions
and make the tradeoff explicit in UI/docs.

## Minimal Checklist

- Use `rc_client`, not low-level `rc_runtime`, unless there is a specific reason.
- Own all rcheevos memory/list lifetimes inside the integration layer.
- Keep a generation id and discard stale async completions.
- Call frame only in active game state.
- Call idle periodically while paused.
- Reapply Hardcore before loading a game.
- Keep memory reads side-effect-free.
- Refresh lists after frame processing, not inside event callbacks.
- Rate-limit Rich Presence reads.
- Validate RA progress before applying it to a loaded state.
- Never log credentials or request bodies.
