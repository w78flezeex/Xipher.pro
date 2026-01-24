# Update Strategy (Desktop)

This document outlines update options for the Xipher desktop client.

## Goals
- Signed, tamper-resistant updates.
- Small, incremental downloads when possible.
- Cross-platform automation via CI.

## Options
1) macOS
- Sparkle (preferred): mature, delta updates, signed feeds.
- Requirements: Apple Developer ID, app signing, hosted appcast feed.

2) Windows
- MSIX: system-managed updates, good enterprise fit.
- Squirrel/WinSparkle: simpler setup, less enterprise policy coverage.
- Requirements: code signing certificate, update feed hosting.

3) Linux
- AppImage + AppImageUpdate: delta updates.
- Deb/RPM via repo + signed packages.

## Recommendation (Beta)
- Ship zip/AppImage bundles via CI artifacts.
- Defer signed auto-updates to a dedicated release phase.

## Follow-ups
- Decide signing authority and distribution channel.
- Add update feed hosting and release automation.
