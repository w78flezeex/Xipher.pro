import * as vscode from 'vscode';
import * as crypto from 'crypto';
import * as http from 'http';
import * as https from 'https';
import { URL } from 'url';

const OAUTH_SECRET_KEY = 'xipherAi.openai.oauthToken';

export class AuthUriHandler implements vscode.UriHandler {
  private emitter = new vscode.EventEmitter<vscode.Uri>();
  public readonly onUri = this.emitter.event;

  handleUri(uri: vscode.Uri) {
    this.emitter.fire(uri);
  }
}

type OAuthToken = {
  access_token: string;
  refresh_token?: string;
  expires_in?: number;
  expires_at?: number;
  token_type?: string;
};

export class OAuthManager {
  private readonly context: vscode.ExtensionContext;
  private readonly uriHandler: AuthUriHandler;

  constructor(context: vscode.ExtensionContext, uriHandler: AuthUriHandler) {
    this.context = context;
    this.uriHandler = uriHandler;
  }

  async signIn() {
    const config = vscode.workspace.getConfiguration('xipherAi.openai');
    const authorizeUrl = (config.get<string>('oauthAuthorizeUrl') || '').trim();
    const tokenUrl = (config.get<string>('oauthTokenUrl') || '').trim();
    const clientId = (config.get<string>('clientId') || '').trim();
    const scopes = (config.get<string>('scopes') || '').trim();

    if (!authorizeUrl || !tokenUrl || !clientId) {
      vscode.window.showErrorMessage('OAuth settings are missing. Configure authorize URL, token URL, and client ID.');
      return;
    }

    const codeVerifier = base64Url(crypto.randomBytes(32));
    const codeChallenge = base64Url(sha256(codeVerifier));
    const state = base64Url(crypto.randomBytes(16));

    const redirectUri = await vscode.env.asExternalUri(
      vscode.Uri.parse(`${vscode.env.uriScheme}://xipher-ai/auth/callback`)
    );

    const authorize = new URL(authorizeUrl);
    authorize.searchParams.set('response_type', 'code');
    authorize.searchParams.set('client_id', clientId);
    authorize.searchParams.set('redirect_uri', redirectUri.toString());
    if (scopes) {
      authorize.searchParams.set('scope', scopes);
    }
    authorize.searchParams.set('code_challenge', codeChallenge);
    authorize.searchParams.set('code_challenge_method', 'S256');
    authorize.searchParams.set('state', state);

    const authPromise = this.waitForCode(state);
    await vscode.env.openExternal(vscode.Uri.parse(authorize.toString()));

    const code = await authPromise;
    if (!code) {
      vscode.window.showErrorMessage('OAuth sign-in canceled.');
      return;
    }

    const token = await exchangeToken(tokenUrl, {
      grant_type: 'authorization_code',
      code,
      redirect_uri: redirectUri.toString(),
      client_id: clientId,
      code_verifier: codeVerifier
    });

    if (token.expires_in && !token.expires_at) {
      token.expires_at = Date.now() + token.expires_in * 1000;
    }

    await this.context.secrets.store(OAUTH_SECRET_KEY, JSON.stringify(token));
    vscode.window.showInformationMessage('OAuth sign-in successful.');
  }

  async signOut() {
    await this.context.secrets.delete(OAUTH_SECRET_KEY);
    vscode.window.showInformationMessage('Signed out.');
  }

  async hasToken(): Promise<boolean> {
    const value = await this.context.secrets.get(OAUTH_SECRET_KEY);
    return Boolean(value);
  }

  async getAccessToken(): Promise<string | undefined> {
    const raw = await this.context.secrets.get(OAUTH_SECRET_KEY);
    if (!raw) {
      return undefined;
    }

    let token = parseToken(raw);
    if (!token) {
      return undefined;
    }

    if (token.expires_at && token.expires_at <= Date.now() - 30000) {
      token = await this.refreshToken(token);
      if (!token) {
        return undefined;
      }
    }

    return token.access_token;
  }

  private async refreshToken(token: OAuthToken): Promise<OAuthToken | undefined> {
    if (!token.refresh_token) {
      return token;
    }

    const config = vscode.workspace.getConfiguration('xipherAi.openai');
    const tokenUrl = (config.get<string>('oauthTokenUrl') || '').trim();
    const clientId = (config.get<string>('clientId') || '').trim();

    if (!tokenUrl || !clientId) {
      return token;
    }

    const refreshed = await exchangeToken(tokenUrl, {
      grant_type: 'refresh_token',
      refresh_token: token.refresh_token,
      client_id: clientId
    });

    if (refreshed.expires_in && !refreshed.expires_at) {
      refreshed.expires_at = Date.now() + refreshed.expires_in * 1000;
    }

    await this.context.secrets.store(OAUTH_SECRET_KEY, JSON.stringify(refreshed));
    return refreshed;
  }

  private waitForCode(state: string): Promise<string | undefined> {
    return new Promise((resolve) => {
      const timeout = setTimeout(() => resolve(undefined), 120000);
      const sub = this.uriHandler.onUri((uri) => {
        const params = new URLSearchParams(uri.query);
        const incomingState = params.get('state');
        const code = params.get('code');
        const error = params.get('error');

        if (incomingState !== state) {
          return;
        }

        clearTimeout(timeout);
        sub.dispose();

        if (error) {
          vscode.window.showErrorMessage(`OAuth error: ${error}`);
          resolve(undefined);
          return;
        }

        resolve(code || undefined);
      });
    });
  }
}

function parseToken(raw: string): OAuthToken | undefined {
  try {
    const data = JSON.parse(raw) as OAuthToken;
    if (!data.access_token) {
      return undefined;
    }
    return data;
  } catch {
    return undefined;
  }
}

function sha256(value: string) {
  return crypto.createHash('sha256').update(value).digest();
}

function base64Url(buffer: Buffer) {
  return buffer
    .toString('base64')
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '');
}

type TokenPayload = Record<string, string>;

async function exchangeToken(tokenUrl: string, payload: TokenPayload): Promise<OAuthToken> {
  const url = new URL(tokenUrl);
  const body = new URLSearchParams(payload).toString();

  const headers: Record<string, string> = {
    'Content-Type': 'application/x-www-form-urlencoded',
    'Content-Length': Buffer.byteLength(body).toString()
  };

  const responseText = await sendRequest(url, body, headers);
  let data: OAuthToken;
  try {
    data = JSON.parse(responseText) as OAuthToken;
  } catch (error) {
    throw new Error('Invalid OAuth token response.');
  }

  if (!data.access_token) {
    throw new Error('OAuth token missing access_token.');
  }

  return data;
}

function sendRequest(url: URL, body: string, headers: Record<string, string>): Promise<string> {
  const isHttps = url.protocol === 'https:';
  const transport = isHttps ? https : http;

  return new Promise((resolve, reject) => {
    const request = transport.request(
      {
        method: 'POST',
        hostname: url.hostname,
        port: url.port ? Number(url.port) : isHttps ? 443 : 80,
        path: url.pathname + url.search,
        headers
      },
      (res: http.IncomingMessage) => {
        const chunks: Buffer[] = [];
        res.on('data', (chunk: Buffer | string) =>
          chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk))
        );
        res.on('end', () => {
          const text = Buffer.concat(chunks).toString('utf8');
          if (res.statusCode && res.statusCode >= 400) {
            reject(new Error(`OAuth error ${res.statusCode}: ${text}`));
            return;
          }
          resolve(text);
        });
      }
    );

    request.on('error', (error: Error) => reject(error));
    request.write(body);
    request.end();
  });
}
