# RetroAchievements認証情報の保存

保存するのはlogin tokenであり、passwordではありません。usernameは通常設定へ保存して構いません。
tokenは可能な限りOSのcredential storeへ保存します。

## 共通契約

```cpp
bool credential_save(const char* service, const char* account, const char* secret);
bool credential_load(const char* service, const char* account, char* secret, size_t secret_size);
bool credential_delete(const char* service, const char* account);
```

推奨値:

- `service`: `org.example.Emulator.RetroAchievements` のようなアプリ固有ID
- `account`: RetroAchievements username
- `secret`: RetroAchievements token

保存tokenでのlogin失敗時はtokenを削除します。

## macOS

Keychainのgeneric passwordを使います。serviceはアプリ固有ID、accountはRA usernameです。

## Windows

Windows Credential ManagerのGeneric Credentialを使います。

target例:

```text
<service>:<username>
```

Credential Managerが使えない特殊環境では、DPAPIで暗号化した設定ファイルを明示的なfallbackに
できます。平文保存は推奨しません。

## Linux

Secret Serviceをlibsecret経由で使うのが基本です。GNOME Keyring、KWallet、
KeePassXCなどに対応できます。

Secret Serviceがない場合、最も安全な既定動作はtokenを保存せず、次回起動時に再loginを求める
ことです。portable build向けにfile fallbackを用意する場合は、明示的に選ばせ、permissionを
`0600` に制限してください。

## ログ規則

ログに出してはいけないもの:

- password
- token
- API key
- POST body
- 認証情報を含み得る完全なURL

「token保存に失敗した」「HTTP requestに失敗した」のような高水準errorだけを出します。
