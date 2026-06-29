# GUIモデル

この文書は、RetroAchievements対応で想定するユーザー向けUIを定義します。
RA runtimeはsnapshotとeventを提供し、アプリがSDL overlay、native window、mobile view、
CLI表示などへ変換します。

OS固有dialogは、file選択、credential保存、権限要求など避けられない場面に限定します。

## 必須画面

### RA設定

- RetroAchievements有効/無効
- usernameまたはaccount選択
- login/logout
- Softcore/Hardcore選択
- 安全な保存が使える場合のみtoken保存
- credential storageの利用可否表示

RAは既定OFFです。passwordはloginにだけ使い、保存しません。
active game中のmode変更は、現在sessionの終了または再起動を要求します。

### Login

- username
- password
- login
- cancel
- authenticating / failed / offline継続などの状態表示

保存tokenでのlogin失敗時はtokenを削除し、LoggedOutへ戻します。

### Session Status

表示するもの:

- disabled / logged out / authenticating / ready / loading / active / offline / disconnected
- game title、RA game id
- Softcore / Hardcore
- username、score
- Rich Presence
- 最後のerrorまたはoffline理由

overlay、status row、achievements window headerなど、アプリに合った場所で表示します。

### Achievements

各achievementに表示するもの:

- badgeまたはplaceholder
- title
- description
- points
- status/bucket
- measured progress / percent
- rarity
- unlocked状態

filterとsortはあると望ましいです。UIはruntime snapshotだけを読み、rcheevos pointerを
直接読まないでください。

### Leaderboards

各leaderboardに表示するもの:

- title
- description
- state/bucket
- tracker value
- score format
- lower-is-better
- top entryまたはcontext

submit時には、submitted score、rank、personal best、total entries、server-provided top
entriesを通知します。

### Game Library

Game Libraryを持つ場合は、RA sessionを開始できる媒体をアプリ側で管理します。

- media import
- media hashとdisplay name
- game grouping
- launch configuration
- 実行用媒体またはsession copy状態
- last played
- 起動時RA mode

RA modeで原本とは別の実行用媒体を使う場合は、その状態がユーザーに分かるようにします。

## Overlay通知

通知は短く、原則として非blockingにします。

- achievement unlocked
- challenge indicator shown/hidden
- achievement progress
- leaderboard started
- leaderboard tracker update
- leaderboard failed
- leaderboard submitted
- disconnected/reconnected
- server error
- offline session started
- Hardcore policy denial
- reset requested

通知には `generation` と `game_id` を持たせ、game変更後の古い通知は捨てます。

## Blocking Prompt

blocking promptは、ユーザー判断なしに進めると危険な場合だけ使います。

- RA mode開始で現在gameを再起動する
- active RA中のmode変更
- 別game media挿入
- secure credential storageなしで保存を要求された
- Hardcore中にload stateが拒否された
- stateが別game/hash/versionに属する

拒否メッセージは「Hardcoreだから」だけではなく、どの操作がなぜ拒否されたかを表示します。

## UI状態

UIは `RaSnapshot.status` から描画します。

```text
Disabled          RA disabled
LoggedOut         login可能
Authenticating    login中
Ready             login済み、game起動待ち
LoadingGame       識別/ロード中
Active            通知、tracker、Rich Presence、一覧を表示
OfflineSession    RA停止、理由を表示
Disconnected      active sessionはあるが通信に問題あり
```

`Disconnected` と `OfflineSession` は分けます。DisconnectedはRA sessionがまだ存在する状態、
OfflineSessionはそのemulator sessionでRA評価/送信を止めた状態です。

## 非GUI host

同じモデルはCLIやtest hostにも使えます。

- status changeを出力
- achievement/leaderboard eventを出力
- snapshotをJSONやlogで公開
- testではfake `UiPresenter` を使う

これにより、最終overlayがなくても統合部分をテストできます。
