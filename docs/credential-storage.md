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

Keychainのgeneric passwordを `SecItemAdd` / `SecItemCopyMatching` /
`SecItemUpdate` / `SecItemDelete` で扱います。serviceはアプリ固有ID、accountは
RA usernameです。`SecKeychainFindGenericPassword` などの `SecKeychain*` APIは
macOS 10.10以降deprecatedなので、新規実装では使いません。

macOS 10.13以上をdeployment targetにする場合、`kSecUseDataProtectionKeychain` は
使わないでください。このキーはmacOS 10.15以降のAPIで、指定するとData Protection
Keychainが対象になります。従来のlogin Keychainに保存済みのtokenとは別扱いになるため、
既存tokenが見つからない原因にもなります。

通常のmacOS Keychainで `kSecAttrAccessible` を使う場合は、queryに
`kSecAttrSynchronizable` も含めます。保存時は同期しないtokenとして
`kSecAttrSynchronizable: kCFBooleanFalse` を明示します。検索、更新、削除時は
`kSecAttrSynchronizable: kSecAttrSynchronizableAny` を入れて、同期属性の有無が異なる
既存itemも拾えるようにします。

保存queryの要点:

```cpp
kSecClass: kSecClassGenericPassword
kSecAttrService: service
kSecAttrAccount: account
kSecValueData: token_data
kSecAttrSynchronizable: kCFBooleanFalse
kSecAttrAccessible: kSecAttrAccessibleAfterFirstUnlock
```

読込queryの要点:

```cpp
kSecClass: kSecClassGenericPassword
kSecAttrService: service
kSecAttrAccount: account
kSecAttrSynchronizable: kSecAttrSynchronizableAny
kSecReturnData: kCFBooleanTrue
kSecMatchLimit: kSecMatchLimitOne
```

`kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` はData Protection Keychainを使う
macOS 10.15以上専用の設計なら選択肢になります。ただしmacOS 10.13/10.14対応を維持する
汎用実装では避け、通常Keychain + `kSecAttrAccessibleAfterFirstUnlock` を使います。

Keychain関連のテストは、実行環境のsandboxやbundle identityの影響を受けます。CLIや
テストランナー内では `errSecParam (-50)` などが返ることがあるため、最終確認は実アプリ、
またはsandbox外の最小テストで保存、読込、削除まで確認します。失敗時のログにはtokenを
出さず、OSStatusだけを含めると原因追跡に役立ちます。

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
