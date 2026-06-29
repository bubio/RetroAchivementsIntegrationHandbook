# xm8mac Adapter Sketch

Japanese version: [README.md](README.md).

This sketch maps the generic handbook rules onto the current xm8mac tree. It is
a companion to xm8mac's own `Documents/RetroAchievements/`
specification, not a replacement for it.

Read this after the portable contracts:

- `docs/en/app-contract.md`
- `docs/en/gui-model.md`
- `docs/en/integration-guide.md`

Observed xm8mac anchors:

- `CMakeLists.txt` already has `XM8_ENABLE_RETROACHIEVEMENTS`.
- `ThirdParty/rcheevos` is already vendored.
- `Source/RA/` currently contains RA dependency probes such as
  `ra_build_info.*`.
- `Source/UI/app.cpp` owns the main run loop, disk open flow, reset, save/load
  state, full speed, and system changes.
- `Source/UI/diskmgr.*` owns D88 open/close/bank handling.
- `Source/ePC-8801MA/vm/pc8801/pc88.cpp` owns PC-88 memory and frame events.

## Target Structure

Add RA runtime code under `Source/RA/` and keep existing VM changes minimal.

Suggested files:

```text
Source/RA/ra_service.h/.cpp
Source/RA/ra_http.h/.cpp
Source/RA/ra_session_policy.h/.cpp
Source/RA/ra_state_store.h/.cpp
Source/RA/ra_models.h
Source/RA/ra_event_queue.h/.cpp
Source/RA/ra_ui_model.h
```

Keep platform-specific HTTP and credential storage behind small interfaces.
Do not put HTTP, SQLite, image decode, credentials, or RA mode logic into
`Source/ePC-8801MA/`.

`ra_ui_model.h` should contain plain host-owned values matching
`docs/en/gui-model.md`: settings state, session status, achievement rows,
leaderboard rows, notifications, and policy denial messages. It should not
contain SDL drawing code.

## App Integration Points

`App` should own or reference the top-level `RaService`.

Connect these `App` methods:

- `Init`: create RA services when `XM8_ENABLE_RETROACHIEVEMENTS` is enabled,
  initialize library paths, and restore RA settings.
- `Run`: drain RA HTTP completions and UI events on the main thread.
- `Draw` or the final video composition point: render RA overlay from the latest
  UI model snapshot after the emulator frame is available.
- VM frame completion inside `Run`: call `RaService::ProcessFrame()` once for
  each completed VM frame, not once per draw or skipped draw.
- paused/background branches inside `Run`: call `RaService::Idle()` about once
  per second while an active RA session is paused.
- `OpenStartupDisks`, `OpenDroppedDisk`, and user disk open paths: route RA mode
  starts through RA media/library handling before opening a D88 in `DiskManager`.
- `Reset`: after `vm->reset()`, call `RaService::NotifyReset()` exactly once for
  the user reset operation.
- `Load` and `Save`: route through `RaSessionPolicy` and `RaStateStore`.
- `FullSpeed`, `ChangeSystem`, and disk/media changes: route through
  `RaSessionPolicy` before mutating emulator state.
- `Deinit`/quit: unload RA game, cancel HTTP, drain callbacks, then destroy
  `rc_client`.

## Disk and Media Flow

xm8mac's RA specification requires RA mode to avoid opening the original D88
directly. Preserve that design:

1. Hash the original D88 as the RA anchor media.
2. Register or find the media in the RA library.
3. Create or reuse the app-owned `working.d88`.
4. Pass only `working.d88` to `DiskManager`.
5. Use the original D88 bytes or path only for RA identification and library
   metadata, never for VM writes.

For M3U or multiple disks, use the first valid D88 as the initial RA
identification anchor, matching the xm8mac RA docs. Same-D88 bank changes do not
require `rc_client_begin_change_media`; separate media changes should go through
the RA media-change path before touching `DiskManager`.

## Frame Loop Pattern

Do this after a real VM frame has completed:

```cpp
if (ra_service && ra_service->IsActive()) {
    ra_service->SetCurrentVm(vm);
    ra_service->ProcessFrame();
    ra_service->SetCurrentVm(nullptr);
    ra_service->RefreshDirtyListsAfterFrame();
    ra_service->RefreshRichPresenceIfDue();
}
```

Do not call `rc_client_do_frame()` from drawing code. xm8mac can skip drawing,
run full speed, or enter background/event wait paths; RA frame evaluation must
follow completed emulation frames.

## Inspection Memory

Add the smallest VM/core API needed for RA memory reads. It should be
side-effect-free and stable for one `rc_client_do_frame()` call.

Sketch:

```cpp
size_t VM::read_ra_inspection_memory(uint32_t address,
                                     uint8_t* buffer,
                                     size_t count) const;
```

or, if the data must live closer to PC88:

```cpp
size_t PC88::read_ra_inspection_memory(uint32_t address,
                                       uint8_t* buffer,
                                       size_t count) const;
```

The method should not insert waits, change device state, acknowledge I/O, or
depend on CPU-side access permissions. If banked RAM, text VRAM, graphic VRAM,
or extended RAM are exposed to RA, define the address ranges in xm8mac's RA
specs and keep the mapping there.

## HTTP Completion

Use the handbook generation pattern:

- increment generation on login state reset, game load, unload, logout, and
  shutdown
- store generation with every RA HTTP request
- cancel host HTTP work on unload/logout
- still complete rcheevos callbacks during orderly cancellation
- discard xm8mac state updates when the completion generation is stale

This protects `App::Run`, disk reloads, and quit paths from slow server
responses mutating the wrong session.

## Event Handling

Inside the rcheevos event callback:

- copy all strings into xm8mac-owned values immediately
- enqueue UI events for the main thread
- set dirty flags for achievements, leaderboards, and user score
- do not enumerate rcheevos lists during `rc_client_do_frame`

After `ProcessFrame()` returns, refresh dirty lists and publish a new snapshot.
This keeps frame-time events out of re-entrant list refresh paths.

Map runtime events to UI events, not directly to drawing calls. For example,
`AchievementTriggered` becomes a notification model with title, description,
badge URL, generation, game id, and expiration time. The SDL overlay consumes
that model on the next draw.

## State and Policy

Use xm8mac's `RaSessionPolicy` design as the single gate for:

- save/load state
- full speed and speed changes
- pseudo fast disk
- reset
- media/system changes
- future debugger or memory tools

For state files, follow xm8mac's `XMRA` chunk design. The important part is the
ordering: validate metadata and checksums before applying VM state; only after
that should `rc_client_deserialize_progress_sized()` be attempted.

## Minimal Bring-Up Order

1. Keep RA disabled by default and prove normal builds are unchanged.
2. Wire `Source/RA` dependency tests and build flags.
3. Add `RaService` with fake HTTP and fake memory tests.
4. Add side-effect-free inspection memory API.
5. Connect login and D88 identify/load without UI polish.
6. Connect frame processing, dirty refresh, and Rich Presence.
7. Add policy gates around reset, state, full speed, and media changes.
8. Add RA state chunk save/load.
9. Add overlay and image cache.
10. Repeat acceptance on each target OS in xm8mac's specified order.
