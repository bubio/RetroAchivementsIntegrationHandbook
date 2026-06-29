# RetroAchievements Credential Storage

RetroAchievements integrations should store the login token, not the password.
The username may be stored in normal settings. The token should go through the
platform credential store when one is available.

## Cross-Platform Contract

Use a tiny host abstraction:

```cpp
bool credential_save(const char* service, const char* account, const char* secret);
bool credential_load(const char* service, const char* account, char* secret, size_t secret_size);
bool credential_delete(const char* service, const char* account);
```

Recommended fields:

- `service`: application-specific reverse-DNS id, such as
  `org.example.Emulator.RetroAchievements`
- `account`: RetroAchievements username
- `secret`: RetroAchievements token

Do not store the user's password. Delete the stored token after a failed token
login so the next launch does not repeatedly retry a bad credential.

## macOS

Use Keychain generic passwords.

Use the emulator's own reverse-DNS application id as the service and the RA
username as the account.

## Windows

Use Windows Credential Manager with a Generic Credential.

Recommended target:

```text
<service>:<username>
```

Implementation outline:

```cpp
CREDENTIALW cred = {};
cred.Type = CRED_TYPE_GENERIC;
cred.TargetName = L"org.example.Emulator.RetroAchievements:username";
cred.UserName = L"username";
cred.CredentialBlob = (LPBYTE)token_bytes;
cred.CredentialBlobSize = token_byte_count;
cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
CredWriteW(&cred, 0);
```

Read with `CredReadW`, convert to UTF-8, then release with `CredFree`.
If Credential Manager is unavailable, DPAPI-encrypted config is a reasonable
explicit fallback. Plaintext config is not recommended.

## Linux

Prefer Secret Service via libsecret.

This covers common desktop setups such as GNOME Keyring, KWallet with Secret
Service support, and KeePassXC. CLI-only or minimal Linux environments may not
have a Secret Service provider.

Implementation outline:

```cpp
secret_password_store_sync(
    schema,
    SECRET_COLLECTION_DEFAULT,
    "RetroAchievements",
    token,
    nullptr,
    &error,
    "account", username,
    nullptr);
```

If Secret Service is unavailable, the safest default is to skip persistence and
ask the user to log in again after restart. A file fallback can be acceptable for
portable builds, but only if it is explicit and uses restrictive permissions,
for example mode `0600`.

## Logging Rules

Never log:

- password
- token
- API key
- POST body
- complete authenticated request URLs

Log high-level failures such as "token was not saved" or "HTTP request failed",
but avoid including secrets in exception context.
