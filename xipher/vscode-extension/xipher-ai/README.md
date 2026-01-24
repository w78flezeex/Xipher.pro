# Xipher AI VS Code Extension

Minimal AI chat extension inspired by Continue/FlexPilot. It supports:

- Chat panel with conversation history
- Explain selection command
- OpenAI API key auth
- Optional OAuth (PKCE) flow for ChatGPT/OpenAI compatible providers

## Setup

1. Install dependencies and build:

```bash
npm install
npm run compile
```

2. Launch the extension from VS Code (Run Extension).

## Commands

- `Xipher AI: Open Chat`
- `Xipher AI: Explain Selection`
- `Xipher AI: Set OpenAI API Key`
- `Xipher AI: Sign In to ChatGPT (OAuth)`
- `Xipher AI: Sign Out of ChatGPT`

## Settings

- `xipherAi.systemPrompt`
- `xipherAi.openai.model`
- `xipherAi.openai.apiBaseUrl`
- `xipherAi.openai.apiKey`
- `xipherAi.openai.useOAuth`
- `xipherAi.openai.oauthAuthorizeUrl`
- `xipherAi.openai.oauthTokenUrl`
- `xipherAi.openai.clientId`
- `xipherAi.openai.scopes`

## OAuth Notes

There is no official ChatGPT OAuth for direct API use. This extension uses a
standard OAuth 2.0 PKCE flow and expects an OAuth provider that returns an
OpenAI-compatible access token.

If you have your own OAuth gateway (or a compatible provider), set the OAuth
URLs and client ID in settings and enable `xipherAi.openai.useOAuth`.
