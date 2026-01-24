# ADR 0001: Session Storage

## Status
Accepted (Beta)

## Context
Desktop needs to persist session cookies for cold-start restore. A secure OS-backed store is preferred.

## Decision
Use OS-backed secure storage for session metadata and cookie blob.

- Windows: DPAPI-protected encrypted JSON file under AppData.
- macOS: Keychain generic password entries.
- Linux: Secret Service (libsecret) entries.

## Rationale
- Avoids plaintext storage of session data.
- No new QtKeychain dependency required for the first production lock.

## Consequences
- Linux builds require libsecret development packages.
- Legacy file-based storage must be migrated once.

## Follow-ups
- Revisit QtKeychain integration if keyring portability becomes an issue.
