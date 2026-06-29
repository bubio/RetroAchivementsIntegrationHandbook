# RetroAchievements統合ガイド

この文書は、rcheevosの `rc_client` を任意のエミュレータへ組み込むための抽象ガイドです。
特定の機種、媒体形式、GUI toolkit、OS API、実装言語には依存しません。

個別エミュレータの仕様やディレクトリ構成は、そのプロジェクト側を正とします。この文書は、
各エミュレータで共通して必要になるライフサイクル、HTTP、メモリ読み取り、Hardcore、
state保存、UI連携の判断を整理します。具体的な接続例は `examples/` を参照してください。

## 推奨構成

3つの層に分けます。

1. **rcheevos層**
   - `rc_client_t` を所有する。
   - login、game identify/load、`do_frame`、`idle`、unload、reset、progress serializeを呼ぶ。
   - rcheevosのlistやeventを、アプリ所有の値へ変換してから外へ出す。

2. **host runtime層**
   - login/load状態、generation id、保留HTTP、現在のエミュレータsession、Hardcore設定、
     Rich Presence、UI snapshotを管理する。
   - `login`、`load_game`、`unload_game`、`process_frame`、`pump`、`reset`、
     `serialize_progress` などを提供する。

3. **emulator adapter層**
   - 副作用のない検査用メモリ読み取りを提供する。
   - media load/unload、frame完了、pause、reset、state save/load、設定変更からruntimeを呼ぶ。
   - UIはsnapshot/eventを描画する。統合層がUIそのものを所有しない。

言語非依存のアプリ側責務は [app-contract.md](app-contract.md)、GUIの想定は
[gui-model.md](gui-model.md) に定義しています。

## ライフサイクル

状態は明示します。

- `LoggedOut`
- `Authenticating`
- `Ready`
- `LoadingGame`
- `Active`
- `OfflineSession`

loginやgame識別が失敗してもエミュレーションは継続します。その媒体は、再ロードまたはRA再試行まで
offline sessionとして扱います。

基本手順:

1. memory callbackとserver callback付きで `rc_client_t` を作る。
2. event handlerとlog handlerを設定する。
3. 保存済みHardcore設定を反映する。
4. passwordまたは保存tokenでloginする。
5. login成功時はtokenだけ保存し、passwordは保存しない。
6. media load時にgeneration idを進め、`LoadingGame`へ遷移し、sessionを設定し、
   Hardcoreを再反映してidentify/loadを呼ぶ。
7. game load成功時にachievement/leaderboardを列挙し、`Active`へ遷移する。
8. 実VMフレーム完了ごとに `rc_client_do_frame` を1回呼び、frame後にdirty listを更新する。
9. pause中は約1秒ごとに `rc_client_idle` を呼ぶ。
10. unload/logout/disposeではgenerationを進め、HTTPをcancelし、rcheevos requestをabortし、
    game listとRich Presenceを消す。

## HTTP

rcheevosはHTTPを実行しません。server callbackでは次を行います。

- request idを割り当てる
- 現在のgeneration idを保存する
- `post_data == null` ならGET、それ以外はPOST
- content type、status code、bodyを維持する
- response size上限を持つ。例: 8MiB
- timeout/cancelをclient errorへ変換する
- password、token、POST body、認証済みURL全体をログに出さない
- `rc_client` 所有threadへcompletionを戻す
- generation不一致のcompletionではアプリ状態を更新しない

このgeneration管理がないと、遅いHTTP応答がmedia unload後や別ゲームload後に戻り、
別sessionを壊す可能性があります。

## メモリ読み取り

RAへ公開するのはCPU busそのものではなく、検査用メモリマップです。

memory callbackは次を満たします。

- 副作用がない
- 1回の `rc_client_do_frame` 中は安定している
- 公開範囲が文書化されている
- wait挿入、I/O acknowledge、bank切替などを起こさない
- bank/mirrorはエミュレータ側で正規化する

検査用メモリの入口は、エミュレータcoreに小さく追加します。HTTP、UI、credential保存、
画像decode、RA mode判定のようなアプリケーション側の関心は、coreへ持ち込まないでください。

## EventとSnapshot

UIがrcheevos構造体を直接読む設計にしません。次のようなhost所有値へ変換します。

- user
- game
- achievements
- leaderboards
- Rich Presence
- Hardcore enabled
- status
- generation id

`rc_client_do_frame` 中にeventが来た場合、その場でachievement/leaderboard listを再列挙せず、
dirty flagだけ立てます。`rc_client_do_frame` が戻った後で更新します。

Rich Presenceは毎フレームではなく、1秒程度の周期で取得します。gameまたはlogin状態が終わったら
即座に消します。

## Hardcoreと操作制限

実績の公正性に影響する操作は、必ず1つのpolicy関数を通します。

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

推奨初期値:

- Softcoreでは通常のstate操作を許可する。
- Hardcoreではload stateを拒否する。
- save state作成は許可してよいが、読み込みは不可。
- ActiveなRA session中のpauseは `rc_client_can_pause` へ委譲する。
- fast-forwardは必ず禁止とは限らないが、policy上の明示的な判断にする。

## State

通常のエミュレータstateとRA progressは分けて考えます。RA対応stateには次を保存します。

- magic/version
- game id
- media hash
- rcheevos version
- emulator core state
- sized `rc_client` progress
- checksum

load時は、エミュレータstateを1バイトも適用する前にmetadataとchecksumを検証します。
既存のstate形式を拡張する場合も、RA progressは明示的なversion、size、checksumを持つ
独立した領域として扱います。

## 最小チェックリスト

- `rc_runtime` 直叩きではなく `rc_client` を使う。
- rcheevos由来のpointer/list寿命を統合層内に閉じる。
- generation idで古いasync completionを捨てる。
- frame評価はActive game中だけ行う。
- pause中はperiodic idleを呼ぶ。
- game load前にHardcore設定を再適用する。
- memory readは副作用なしにする。
- event callback中にlistを再列挙しない。
- Rich Presenceをrate limitする。
- state適用前にRA metadataを検証する。
- credentialやrequest bodyをログに出さない。
