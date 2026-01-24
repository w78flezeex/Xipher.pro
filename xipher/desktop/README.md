# Xipher Desktop (Qt 6 / C++17)

Production-grade desktop client for Xipher, aligned with the web UX and adapted for desktop patterns.

## Docs

- Update strategy: `desktop/docs/UpdateStrategy.md`
- ADRs: `desktop/docs/ADR/`

## Build

Prerequisites:
- Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2, Network, WebSockets, Widgets, Test)
- CMake 3.21+
- Linux: `libsecret-1-dev` (Secret Service) and runtime `libsecret-1-0`

Configure + build:

```
cmake -S xipher/desktop -B xipher/desktop/out/build/dev -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build xipher/desktop/out/build/dev
ctest --test-dir xipher/desktop/out/build/dev --output-on-failure
```

Tests:
- File dialog tests can be forced to return preset files via `XIPHER_TEST_DIALOG_FILES`.

Run:
- Launch the built binary from `xipher/desktop/out/build/dev`.
- Set `XIPHER_BASE_URL` if your server is not `https://xipher.pro`.
- Optional flags: `XIPHER_ENABLE_CALLS`, `XIPHER_ENABLE_BOTS`, `XIPHER_ENABLE_ADMIN`, `XIPHER_ENABLE_MARKETPLACE`.
- Optional upload limit override: `XIPHER_UPLOAD_MAX_MB` (default 25).

## Contract Notes (API + WS)

Sources:
- `src/server/request_handler.cpp`
- `src/server/http_server.cpp`
- `web/js/login.js`
- `web/js/session.js`
- `web/js/chat.js`

### HTTP API

`POST /api/login`
- Request JSON: `{ "username": "<string>", "password": "<string>" }`
- Response (success):
  `{ "success": true, "message": "...", "data": { "user_id": "...", "token": "...", "username": "...", "is_premium": "true|false", "premium_plan": "...", "premium_expires_at": "..." } }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/validate-token`
- Request JSON: `{ "token": "<string>" }` (web client sends `{}` and server injects cookie token)
- Response (success):
  `{ "success": true, "message": "...", "data": { "user_id": "...", "username": "...", "is_premium": "true|false", "premium_plan": "...", "premium_expires_at": "..." } }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/chats`
- Request JSON: `{ "token": "<string>" }`
- Response (success): `{ "success": true, "chats": [ { "id": "...", "name": "...", "display_name": "...", "avatar": "...", "avatar_url": "...", "lastMessage": "...", "time": "HH:MM", "unread": <int>, "online": <bool>, "last_activity": "...", "is_saved_messages": <bool>, "is_bot": <bool>, "is_premium": <bool> } ] }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/messages`
- Request JSON: `{ "token": "<string>", "friend_id": "<chatId>" }`
- Response (success): `{ "success": true, "messages": [ { "id": "...", "sender_id": "...", "sent": <bool>, "status": "sent|delivered|read", "is_read": <bool>, "is_delivered": <bool>, "content": "...", "message_type": "text", "file_path": "...", "file_name": "...", "file_size": <int>, "reply_to_message_id": "...", "time": "HH:MM", "is_pinned": <bool> } ] }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/send-message`
- Request JSON: `{ "token": "<string>", "receiver_id": "<chatId>", "content": "<text>", "message_type": "text", "reply_to_message_id": "<optional>", "temp_id": "<optional>" }`
- Response (success): `{ "success": true, "message": "...", "message_id": "...", "temp_id": "...", "created_at": "...", "time": "HH:MM", "status": "sent", "is_read": false, "is_delivered": false, "content": "...", "message_type": "text", "file_path": "...", "file_name": "...", "file_size": <int>, "reply_to_message_id": "..." }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/upload-file`
- Request JSON: `{ "token": "<string>", "file_data": "<base64>", "file_name": "<string>" }`
- Response (success): `{ "success": true, "file_path": "/files/<name>", "file_name": "<string>", "file_size": <int> }`
- Response (error): `{ "success": false, "message": "..." }`

`POST /api/delete-message`
- Request JSON: `{ "token": "<string>", "message_id": "<string>" }`
- Response (success): `{ "success": true, "message": "..." }`
- Response (error): `{ "success": false, "message": "..." }`

### WebSocket

Endpoint:
- `ws://<host>/ws` or `wss://<host>/ws`

Auth message (client -> server):
- `{ "type": "auth", "token": "<token|cookie>" }` (web uses token placeholder `cookie`)

Server events:
- `auth_success`
- `auth_error` `{ "type": "auth_error", "error": "Invalid token" }`
- `new_message` payload includes: `chat_type`, `chat_id`, `id`/`message_id`, `temp_id`, `sender_id`, `receiver_id`, `content`, `message_type`, `file_path`, `file_name`, `file_size`, `reply_to_message_id`, `created_at`, `time`, `status`, `is_read`, `is_delivered`
- `typing` payload includes: `chat_type`, `chat_id`, `from_user_id`, `from_username`, `is_typing`
- `message_delivered` / `message_read` payload includes: `message_id`, `chat_id`, `from_user_id`
- `message_deleted` payload includes: `message_id`, `chat_id`
- `avatar_updated` payload includes: `user_id`, `avatar_url`

Client events:
- `typing`: `{ "type": "typing", "token": "<token>", "chat_type": "chat", "chat_id": "<chatId>", "is_typing": "1|0" }`
- `message_delivered`: `{ "type": "message_delivered", "token": "<token>", "message_id": "<id>" }`
- `message_read`: `{ "type": "message_read", "token": "<token>", "message_id": "<id>" }`

## Morning-ready checklist

- Theme toggle works (dark/light/system).
- Cold start restores session (valid cookie -> Shell).
- Login works and session switches to shell.
- Chats list loads.
- Messages list loads for selected chat.
- Send message works (optimistic + reconcile).
- Drag & drop upload works and sends attachment message.
- WebSocket connects and typing/receipts update.

## Known limitations

- Calls/WebRTC are disabled (feature flag OFF).
- Offline cache is not implemented.
- Attachment types are limited to text/file/image.
- Uploads are base64-encoded; large files are limited by `XIPHER_UPLOAD_MAX_MB`.

## ASSUMPTIONS

- Fonts are provided in `desktop/assets/fonts` (not bundled in repo).
- Session token is cookie-based and stored in OS secure storage.
- Default API base URL is `https://xipher.pro`.
- Chat list ordering follows server order without client-side sorting.
