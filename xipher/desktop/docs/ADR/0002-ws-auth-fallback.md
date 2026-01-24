# ADR 0002: WebSocket Auth Fallback

## Status
Accepted

## Context
Server supports cookie auth and auth messages. Desktop must interoperate with web behavior.

## Decision
Implement auth stages:
1) Cookie-only connection.
2) Auth message with token placeholder ("cookie").
3) Reconnect with header token (if available).

## Rationale
- Matches web flow where token placeholder is used with cookies.
- Provides compatibility without forcing headers.

## Consequences
- Slightly longer auth time in cookie-only mode (timeout before fallback).
- Requires careful retry/backoff to avoid loops.
