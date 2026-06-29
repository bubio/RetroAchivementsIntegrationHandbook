# rcheevos Emulator Integration Handbook

このリポジトリは
[rcheevos](https://github.com/RetroAchievements/rcheevos) の代替SDKではありません。
任意のエミュレータへRetroAchievements対応を組み込むための、実装手引きと最小サンプルです。

rcheevosは、RetroAchievements対応に必要な中核機能を既に提供しています。この文書群は、
rcheevosが意図的にアプリケーションへ委ねている部分を、機種、媒体形式、GUI toolkit、
実装言語に依存しない形で整理します。具体的な接続例は `examples/` に分離しています。

## 内容

- [docs/integration-guide.md](docs/integration-guide.md): rcheevos統合の全体方針
- [docs/app-contract.md](docs/app-contract.md): アプリ側が実装する言語非依存の契約
- [docs/gui-model.md](docs/gui-model.md): 設定、ログイン、通知、実績一覧などのGUIモデル
- [docs/credential-storage.md](docs/credential-storage.md): OS別のトークン保存方針
- [examples/minimal-c-client](examples/minimal-c-client): host側ライフサイクルの最小Cサンプル
- [examples/xm8mac-adapter-sketch](examples/xm8mac-adapter-sketch): 具体的なエミュレータ接続例
- [README.en.md](README.en.md): English version

## 位置づけ

rcheevosは、実績条件の評価、ゲーム識別、Leaderboard、Rich Presence、RA API要求生成を
担当します。このリポジトリは、rcheevosが意図的にアプリへ委ねている部分を扱います。

- frame、idle、load、unload、reset、progress保存をいつ呼ぶか
- 非同期HTTP完了を安全に戻す方法
- 副作用のない検査用メモリマップをどう公開するか
- media変更後の古いcallbackをどう捨てるか
- UI向けsnapshot/eventをどう構成するか
- passwordを保存せずtokenをどう保持するか

汎用契約と個別エミュレータの接続例は分けています。別エミュレータや別言語へ応用する場合は、
まず `docs/app-contract.md` と `docs/gui-model.md` を読み、自分のアプリ構造へ
portを対応づけてください。その後、必要に応じて `examples/` の具体例を比較材料として使います。
