# xm8macアダプタスケッチ

この文書は、汎用ハンドブックのRA統合規則を、現在のxm8macツリーへ対応づけるための
スケッチです。xm8mac側の `Documents/RetroAchievements/` 仕様を置き換えるものではなく、
実装時の接続点を整理する補助文書です。

先に読む文書:

- `docs/app-contract.md`
- `docs/gui-model.md`
- `docs/integration-guide.md`

## 確認済みのxm8mac側接続点

- `CMakeLists.txt` に `XM8_ENABLE_RETROACHIEVEMENTS` がある。
- `ThirdParty/rcheevos` は既に同梱済み。
- `Source/RA/` には `ra_build_info.*` など依存物確認コードがある。
- `Source/UI/app.cpp` がmain loop、disk open、reset、save/load state、full speed、
  system changeを所有している。
- `Source/UI/diskmgr.*` がD88 open/close/bank処理を所有している。
- `Source/ePC-8801MA/vm/pc8801/pc88.cpp` がPC-88 memoryとframe eventを所有している。

## 推奨ファイル構成

RA runtimeは `Source/RA/` に置き、VM側の変更は最小にします。

```text
Source/RA/ra_service.h/.cpp
Source/RA/ra_http.h/.cpp
Source/RA/ra_session_policy.h/.cpp
Source/RA/ra_state_store.h/.cpp
Source/RA/ra_models.h
Source/RA/ra_event_queue.h/.cpp
Source/RA/ra_ui_model.h
```

platform別HTTPとcredential保存は小さなinterfaceの背後に隠します。
HTTP、SQLite、画像decode、credential、RA mode判定を `Source/ePC-8801MA/` へ入れないでください。

`ra_ui_model.h` は描画コードではなく、`docs/gui-model.md` に対応するhost所有値を
定義します。settings状態、session status、achievement行、leaderboard行、通知、
policy拒否messageなどです。

## Appへの接続点

`App` はtop-levelの `RaService` を所有または参照します。

- `Init`: `XM8_ENABLE_RETROACHIEVEMENTS` 有効時にRA serviceを作り、library pathとRA設定を復元する。
- `Run`: main threadでRA HTTP completionとUI eventをdrainする。
- `Run` 内の実VM frame完了後: draw回数ではなく、完了したVM frameごとに
  `RaService::ProcessFrame()` を1回呼ぶ。
- pause/background分岐: active RA session中は約1秒ごとに `RaService::Idle()` を呼ぶ。
- `Draw` または最終video合成点: emulator frameの上にRA overlayを描画する。
- `OpenStartupDisks`、`OpenDroppedDisk`、user disk open: `DiskManager` へD88を渡す前に
  RA media/library処理を通す。
- `Reset`: `vm->reset()` 後に `RaService::NotifyReset()` を同一操作につき1回だけ呼ぶ。
- `Load` / `Save`: `RaSessionPolicy` と `RaStateStore` を通す。
- `FullSpeed`、`ChangeSystem`、disk/media change: emulator状態を変更する前に
  `RaSessionPolicy` を通す。
- `Deinit` / quit: RA game unload、HTTP cancel、callback drain、`rc_client` destroyの順で片付ける。

## DiskとMedia

xm8macのRA仕様どおり、RA modeでは原本D88を直接VMへ渡しません。

1. 原本D88をRA anchor mediaとしてhashする。
2. RA libraryへmediaを登録または検索する。
3. アプリ管理下の `working.d88` を作成または再利用する。
4. `DiskManager` へ渡すのは `working.d88` だけにする。
5. 原本D88 bytes/pathはRA識別とlibrary metadataにだけ使い、VM writeには使わない。

M3Uまたは複数diskでは、xm8mac RA仕様に合わせて最初の有効D88を初回RA識別anchorにします。
同一D88内のbank変更では `rc_client_begin_change_media` は不要です。別mediaへの変更は、
`DiskManager` を触る前にRA media-change処理を通します。

## Frame Loop

実VM frame完了後に呼びます。

```cpp
if (ra_service && ra_service->IsActive()) {
    ra_service->SetCurrentVm(vm);
    ra_service->ProcessFrame();
    ra_service->SetCurrentVm(nullptr);
    ra_service->RefreshDirtyListsAfterFrame();
    ra_service->RefreshRichPresenceIfDue();
}
```

`rc_client_do_frame()` を描画処理から呼ばないでください。xm8macは描画skip、full speed、
background/event waitを持つため、RA frame評価は描画ではなく完了したemulation frameへ
結びつけます。

## 検査用メモリ

RA memory read用に、VM/coreへ最小APIを追加します。副作用がなく、1回の
`rc_client_do_frame()` 中で安定している必要があります。

```cpp
size_t VM::read_ra_inspection_memory(uint32_t address,
                                     uint8_t* buffer,
                                     size_t count) const;
```

またはPC88に近い方が自然なら:

```cpp
size_t PC88::read_ra_inspection_memory(uint32_t address,
                                       uint8_t* buffer,
                                       size_t count) const;
```

このAPIはwait挿入、device状態変更、I/O acknowledge、CPU access権限依存を起こしません。
banked RAM、text VRAM、graphic VRAM、extended RAMをRAへ公開する場合は、xm8mac RA仕様に
address rangeを定義します。

## HTTP Completion

ハンドブックで定義したgeneration patternを使います。

- login reset、game load、unload、logout、shutdownでgenerationを進める
- RA HTTP requestごとにgenerationを保存する
- unload/logoutでhost HTTPをcancelする
- orderly cancellation中もrcheevos callbackは完了させる
- generation不一致のcompletionではxm8mac状態を更新しない

これにより、遅いserver responseが `App::Run`、disk reload、quit pathで別sessionを
壊すことを防ぎます。

## Event Handling

rcheevos event callback内では次だけ行います。

- 文字列をxm8mac所有値へ即座にcopyする
- main thread向けUI eventへenqueueする
- achievements、leaderboards、user scoreのdirty flagを立てる
- `rc_client_do_frame` 中にrcheevos listを再列挙しない

`ProcessFrame()` が戻った後でdirty listを更新し、新しいsnapshotをpublishします。

runtime eventは描画呼び出しではなくUI modelへ変換します。例えば `AchievementTriggered` は、
title、description、badge URL、generation、game id、expire timeを持つ通知modelになります。
SDL overlayは次のdrawでそのmodelを描画します。

## StateとPolicy

xm8macの `RaSessionPolicy` を、次の操作の唯一の入口にします。

- save/load state
- full speedと速度変更
- pseudo fast disk
- reset
- media/system change
- 将来のdebuggerやmemory tool

state fileはxm8macの `XMRA` chunk仕様に従います。ここで重要なのは保存/復元の順序です。
metadataとchecksumを検証してからVM stateを適用し、その後でのみ
`rc_client_deserialize_progress_sized()` を試します。

## 最小実装順

1. RA既定OFFを維持し、Normal buildが変わらないことを確認する。
2. `Source/RA` 依存物testとbuild flagを接続する。
3. fake HTTP / fake memory test付きで `RaService` を追加する。
4. 副作用なしのinspection memory APIを追加する。
5. UI polishなしでloginとD88 identify/loadを接続する。
6. frame processing、dirty refresh、Rich Presenceを接続する。
7. reset、state、full speed、media changeにpolicy gateを追加する。
8. RA state chunk save/loadを追加する。
9. overlayとimage cacheを追加する。
10. xm8mac仕様の順序で各OSの受入を繰り返す。
