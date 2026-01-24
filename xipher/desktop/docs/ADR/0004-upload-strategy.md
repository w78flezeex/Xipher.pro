# ADR 0004: Upload Strategy

## Status
Accepted (Beta)

## Context
Server `/api/upload-file` expects JSON payload with `file_data` as base64. Multipart is not supported in current backend.

## Decision
Keep base64 JSON uploads and enforce a client-side size limit to prevent memory spikes.

- Default limit: 25 MB.
- Configurable via `XIPHER_UPLOAD_MAX_MB`.
- Reject oversize files before reading into memory.
- Retry transient failures (network/5xx) with capped backoff.

## Rationale
- Matches existing backend contract without server changes.
- Prevents runaway memory usage on large files.
- Provides predictable behavior for users and CI.

## Consequences
- Large files require server and client changes to support multipart streaming.
- Base64 encoding adds overhead (~33%).

## Follow-ups
- Implement multipart upload if backend adds support.
- Add server-side limits surfaced in API responses.
