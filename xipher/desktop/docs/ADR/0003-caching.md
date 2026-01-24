# ADR 0003: Image Cache

## Status
Accepted

## Context
Avatars and image attachments should not be reloaded on every view.

## Decision
Use QNetworkDiskCache via a QQmlNetworkAccessManagerFactory for QML images.

## Rationale
- Simple integration with Qt QML.
- No additional dependencies.

## Consequences
- Relies on HTTP cache headers when present.
- Memory cache is handled by QML internally.
