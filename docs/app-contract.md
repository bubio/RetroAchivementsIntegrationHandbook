# アプリケーション契約

この文書は、rcheevosの周囲でエミュレータアプリが提供するべき契約を定義します。
C++、Swift、C#、F#、Rust、Kotlinなど、どの言語でも同じ責務へ対応づけられるように
言語非依存で書いています。

## 境界

RA runtimeが所有するもの:

- rcheevos `rc_client` 状態
- login / game load状態
- async無効化用generation id
- achievement、leaderboard、Rich Presence、user snapshot
- 保留中のrcheevos operation
- event正規化

アプリが所有するもの:

- UI描画
- HTTP実装
- credential保存
- media選択と実行用媒体方針
- emulator memory inspection
- main thread dispatch
- badge等の画像取得/デコード
- state file配置とemulator core state保存
- emulator状態を変更する前のpolicy判定

UIへrcheevos pointerを渡してはいけません。rcheevos由来の文字列、list、eventは、
必ずアプリ所有の値へ変換してから外へ出します。

## 必須Port

| Port | アプリ実装 | 用途 |
|---|---:|---|
| `HttpTransport` | 必須 | RA API要求を非同期送信し、status/bodyを返す。 |
| `CredentialStore` | 必須 | login tokenの保存、取得、削除。passwordは保存しない。 |
| `MainThreadDispatcher` | 必須 | HTTP completionとUI eventを所有threadへ戻す。 |
| `MemoryInspector` | 必須 | `rc_client_do_frame` とRich Presence用の副作用なしメモリ読み取り。 |
| `MediaProvider` | 必須 | media bytes/path、hash、実行用媒体、media change情報を提供。 |
| `SessionPolicy` | 必須 | reset、pause、load state、full speed、media change等の許可判定。 |
| `StateStore` | 必須 | emulator stateとRA progress metadataを保存/検証する。 |
| `UiPresenter` | 必須 | settings、status、achievement、leaderboard、通知、errorを描画。 |
| `ImageProvider` | 任意 | badge、game image、avatarを取得/cacheする。 |
| `Logger` | 必須 | secretを含まない診断ログ。 |

emulator coreへ触る必要があるのは基本的に `MemoryInspector` だけです。
HTTP、credential、SQLite、画像、UIはcoreへ持ち込まないでください。

## Snapshot

UIは原則としてimmutableなsnapshotを描画します。

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

achievement、leaderboard、user、gameもhost所有の単純な値として持ちます。
rawなrcheevos event idはdebug用に残してよいですが、UIにはアプリ側event名を渡します。

## Runtime Event

runtimeは次のeventを発行できるようにします。

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

game中のeventには `generation` と `game_id` を付けます。UIは現在snapshotとgenerationが
合わないeventを捨てます。

## Threading

- `rc_client` とRA runtime stateを所有するthreadを1つ決める。
- HTTP workerはemulator、UI、`rc_client` を直接変更しない。
- HTTP completionは `MainThreadDispatcher` 経由で戻す。
- async requestにはgeneration idを保存する。
- login reset、game load、unload、logout、destroyでgenerationを進める。
- 古いgenerationのcompletionはアプリ状態を更新しない。
- shutdown順序は、frame停止、HTTP cancel、rcheevos async abort、callback drain、
  `rc_client_destroy`。

## 言語別対応

| 概念 | C/C++ | Swift | C# / F# | Rust |
|---|---|---|---|---|
| runtime handle | class / opaque pointer | final class | `IDisposable` class | lifetime付きstruct |
| host port | abstract class / function table | protocol | interface / delegate | trait |
| event | struct queue | associated value付きenum | discriminated union / event | enum |
| snapshot | value struct | struct | record | struct |
| async HTTP | worker + queue | `URLSession` + main actor | `HttpClient` + synchronization context | task + channel |

言語からrcheevosを直接扱いにくい場合だけ、薄いC ABI bridgeを追加します。
その場合も、このhost contractは変えないでください。
