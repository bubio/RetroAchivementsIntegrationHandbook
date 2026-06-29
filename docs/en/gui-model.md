# GUI Model

This document describes the user-facing RetroAchievements UI expected by the
integration. The RA runtime should provide snapshots and events; the application
decides whether to render them as SDL overlay screens, native desktop windows,
mobile views, or command-line status. Native OS dialogs should be limited to
platform-required file selection, credential storage, or permission flows.

## Required Screens

### RA Settings

Purpose: opt in to RA and choose the execution mode before starting a session.

Controls:

- enable RetroAchievements
- username field or account selector
- login/logout command
- Softcore/Hardcore mode selector
- remember token toggle only if secure storage is available
- status text for credential storage availability
- link or command to open achievements/library screen

Rules:

- RA is disabled by default.
- Password entry is used only for login and is never persisted.
- Failed stored-token login should clear the stored token and return to logged
  out.
- Switching mode during an active game should require ending or restarting the
  current RA session.

### Login Prompt

Purpose: authenticate when no valid token exists.

Fields:

- username
- password

Actions:

- log in
- cancel

Feedback:

- authenticating
- login failed
- secure token storage unavailable
- offline/session can continue without RA when applicable

### Session Status

Purpose: give the user a compact view of the current RA state.

Show:

- disabled/logged out/authenticating/ready/loading/active/offline/disconnected
- active game title and RA game id
- Softcore or Hardcore
- username and score
- Rich Presence text
- last error or offline reason

This can be a small overlay, menu panel, status row, or achievements-window
header.

### Achievements

Purpose: browse current game achievements.

Show per achievement:

- badge image or placeholder
- title
- description
- points
- status/bucket
- measured progress and percent
- rarity
- unlocked state

Expected controls:

- filter by bucket/status
- sort by original order, title, points, progress, rarity, status
- refresh only from runtime snapshots, not direct rcheevos pointers

### Leaderboards

Purpose: show active and inactive leaderboards and current server context.

Show per leaderboard:

- title
- description
- state/bucket
- tracker value
- score format
- lower-is-better marker
- top entry or fetched context

When a leaderboard is submitted, show:

- submitted score
- rank
- personal best
- total entries
- server-provided top entries when available

### Game Library

Purpose: let users start RA sessions from known media and preserve the
emulator's execution-media policy.

- media import
- media hash and display name
- game grouping
- launch configuration
- execution media or session-copy status
- last played time
- RA mode for launch

The normal emulator file-open path may still exist, but RA mode should make it
clear when execution uses a session copy or transformed media instead of the
original user-selected file.

## Overlay Notifications

Notifications should be short and non-blocking unless user action is required.

Recommended notifications:

- achievement unlocked
- challenge indicator shown/hidden
- achievement progress update
- leaderboard started
- leaderboard tracker update
- leaderboard failed
- leaderboard submitted
- Rich Presence changed only in status UI, not as a toast
- disconnected/reconnected
- server error
- offline session started
- Hardcore policy denial
- reset requested

Each notification should carry `generation` and `game_id`. Drop stale
notifications when the game changes.

## Blocking Prompts

Use blocking prompts only when the next emulator action is ambiguous or
destructive:

- entering RA mode may restart the current game
- changing mode during active RA requires session restart
- inserting media from another RA game ends the current RA session
- secure credential storage is unavailable and the user requested persistence
- loading state is denied in Hardcore
- loading a state belongs to a different game/hash/version

Policy-denied prompts should explain the exact blocked operation, not just say
"Hardcore mode".

## UI State Machine

The UI should render from `RaSnapshot.status`:

```text
Disabled
  Settings: RA disabled
  Overlay: none

LoggedOut
  Settings/Login: login available
  Overlay: optional "not logged in"

Authenticating
  Settings/Login: busy
  Overlay: optional spinner/status

Ready
  Settings: logged in
  Library: launch available

LoadingGame
  Overlay: identifying/loading
  Controls: avoid duplicate launch

Active
  Overlay: notifications, trackers, Rich Presence
  Achievements/Leaderboards: populated

OfflineSession
  Overlay: offline reason
  RA evaluation: stopped

Disconnected
  Overlay: disconnected warning
  RA evaluation: local runtime may continue according to rcheevos state
```

If the runtime distinguishes `ActiveDisconnected` from `OfflineSession`, UI
should show them differently: disconnected means the active RA session still
exists but network is impaired; offline means RA evaluation/submission has been
stopped for this emulator session.

## Layout Guidance

For emulator overlays:

- keep notifications away from critical gameplay areas when possible
- allow users to dismiss or let them expire
- keep text short and wrap safely at the emulator logical resolution
- use placeholders for missing badge images
- cache decoded images with a clear size limit
- avoid blocking the VM unless a policy decision requires it

For desktop/native views:

- achievements and leaderboards can be separate tabs
- settings and login should share account state
- state/policy errors should be visible near the command that caused them

For mobile:

- avoid hover-only affordances
- make login and settings touch-friendly
- handle background lifecycle by showing the current RA status on resume

## Accessibility and Localization

- Do not rely on badge color alone; include text state.
- Keep all RA server text as UTF-8 internally.
- Validate or replace characters unsupported by the emulator font.
- Keep UI strings separate from runtime event names.
- Error messages should be localizable and should not include secrets or local
  file paths unless the user explicitly needs a path to resolve the issue.

## Non-GUI Hosts

The same model works for command-line or headless test hosts:

- print status changes
- print achievement and leaderboard events
- expose snapshots as JSON or logs
- use fake `UiPresenter` implementations in tests

This keeps the integration testable even before the final overlay is complete.
