# ADR 0005: File Dialog Fallback

## Status
Accepted (Beta)

## Context
`QtQuick.Dialogs` is not always available or stable across platforms in CI and headless environments.

## Decision
Expose a C++ `FileDialogService` using `QFileDialog` and bind it into QML.

- QML calls `FileDialogService.openFiles()`.
- Service emits `filesSelected` or `canceled`.
- Tests can inject file paths via `XIPHER_TEST_DIALOG_FILES`.

## Rationale
- Uses native dialogs where available.
- Works consistently in CI and avoids QML module availability issues.

## Consequences
- Requires Qt Widgets linkage.
- Dialog is opened from C++ instead of QML component.

## Follow-ups
- Consider exposing file filters and multi-mode options in the service API.
