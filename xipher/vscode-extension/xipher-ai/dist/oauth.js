"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.OAuthManager = exports.AuthUriHandler = void 0;
const vscode = __importStar(require("vscode"));
const crypto = __importStar(require("crypto"));
const http = __importStar(require("http"));
const https = __importStar(require("https"));
const url_1 = require("url");
const OAUTH_SECRET_KEY = 'xipherAi.openai.oauthToken';
class AuthUriHandler {
    constructor() {
        this.emitter = new vscode.EventEmitter();
        this.onUri = this.emitter.event;
    }
    handleUri(uri) {
        this.emitter.fire(uri);
    }
}
exports.AuthUriHandler = AuthUriHandler;
class OAuthManager {
    constructor(context, uriHandler) {
        this.context = context;
        this.uriHandler = uriHandler;
    }
    async signIn() {
        const config = vscode.workspace.getConfiguration('xipherAi.openai');
        const authorizeUrl = (config.get('oauthAuthorizeUrl') || '').trim();
        const tokenUrl = (config.get('oauthTokenUrl') || '').trim();
        const clientId = (config.get('clientId') || '').trim();
        const scopes = (config.get('scopes') || '').trim();
        if (!authorizeUrl || !tokenUrl || !clientId) {
            vscode.window.showErrorMessage('OAuth settings are missing. Configure authorize URL, token URL, and client ID.');
            return;
        }
        const codeVerifier = base64Url(crypto.randomBytes(32));
        const codeChallenge = base64Url(sha256(codeVerifier));
        const state = base64Url(crypto.randomBytes(16));
        const redirectUri = await vscode.env.asExternalUri(vscode.Uri.parse(`${vscode.env.uriScheme}://xipher-ai/auth/callback`));
        const authorize = new url_1.URL(authorizeUrl);
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
    async hasToken() {
        const value = await this.context.secrets.get(OAUTH_SECRET_KEY);
        return Boolean(value);
    }
    async getAccessToken() {
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
    async refreshToken(token) {
        if (!token.refresh_token) {
            return token;
        }
        const config = vscode.workspace.getConfiguration('xipherAi.openai');
        const tokenUrl = (config.get('oauthTokenUrl') || '').trim();
        const clientId = (config.get('clientId') || '').trim();
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
    waitForCode(state) {
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
exports.OAuthManager = OAuthManager;
function parseToken(raw) {
    try {
        const data = JSON.parse(raw);
        if (!data.access_token) {
            return undefined;
        }
        return data;
    }
    catch {
        return undefined;
    }
}
function sha256(value) {
    return crypto.createHash('sha256').update(value).digest();
}
function base64Url(buffer) {
    return buffer
        .toString('base64')
        .replace(/\+/g, '-')
        .replace(/\//g, '_')
        .replace(/=+$/, '');
}
async function exchangeToken(tokenUrl, payload) {
    const url = new url_1.URL(tokenUrl);
    const body = new URLSearchParams(payload).toString();
    const headers = {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Content-Length': Buffer.byteLength(body).toString()
    };
    const responseText = await sendRequest(url, body, headers);
    let data;
    try {
        data = JSON.parse(responseText);
    }
    catch (error) {
        throw new Error('Invalid OAuth token response.');
    }
    if (!data.access_token) {
        throw new Error('OAuth token missing access_token.');
    }
    return data;
}
function sendRequest(url, body, headers) {
    const isHttps = url.protocol === 'https:';
    const transport = isHttps ? https : http;
    return new Promise((resolve, reject) => {
        const request = transport.request({
            method: 'POST',
            hostname: url.hostname,
            port: url.port ? Number(url.port) : isHttps ? 443 : 80,
            path: url.pathname + url.search,
            headers
        }, (res) => {
            const chunks = [];
            res.on('data', (chunk) => chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk)));
            res.on('end', () => {
                const text = Buffer.concat(chunks).toString('utf8');
                if (res.statusCode && res.statusCode >= 400) {
                    reject(new Error(`OAuth error ${res.statusCode}: ${text}`));
                    return;
                }
                resolve(text);
            });
        });
        request.on('error', (error) => reject(error));
        request.write(body);
        request.end();
    });
}
//# sourceMappingURL=oauth.js.map