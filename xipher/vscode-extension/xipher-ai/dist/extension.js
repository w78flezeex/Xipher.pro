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
exports.deactivate = exports.activate = void 0;
const vscode = __importStar(require("vscode"));
const openaiClient_1 = require("./openaiClient");
const oauth_1 = require("./oauth");
const OPENAI_KEY_SECRET = 'xipherAi.openai.apiKey';
let chatPanel;
function activate(context) {
    const uriHandler = new oauth_1.AuthUriHandler();
    context.subscriptions.push(vscode.window.registerUriHandler(uriHandler));
    const oauthManager = new oauth_1.OAuthManager(context, uriHandler);
    context.subscriptions.push(vscode.commands.registerCommand('xipherAi.openChat', () => {
        chatPanel = ChatPanel.createOrShow(context, oauthManager);
    }));
    context.subscriptions.push(vscode.commands.registerCommand('xipherAi.signInChatGpt', async () => {
        await oauthManager.signIn();
        chatPanel?.postAuthState();
    }));
    context.subscriptions.push(vscode.commands.registerCommand('xipherAi.signOutChatGpt', async () => {
        await oauthManager.signOut();
        chatPanel?.postAuthState();
    }));
    context.subscriptions.push(vscode.commands.registerCommand('xipherAi.setOpenAiKey', async () => {
        const key = await vscode.window.showInputBox({
            prompt: 'Enter OpenAI API key',
            password: true,
            ignoreFocusOut: true
        });
        if (key) {
            await context.secrets.store(OPENAI_KEY_SECRET, key.trim());
            vscode.window.showInformationMessage('OpenAI API key saved.');
            chatPanel?.postAuthState();
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('xipherAi.explainSelection', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showInformationMessage('No active editor.');
            return;
        }
        const selection = editor.selection;
        const text = editor.document.getText(selection).trim();
        if (!text) {
            vscode.window.showInformationMessage('Select code to explain.');
            return;
        }
        const panel = ChatPanel.createOrShow(context, oauthManager);
        await panel.sendUserMessage(`Explain this code:\n\n${text}`, true);
    }));
}
exports.activate = activate;
function deactivate() {
    if (chatPanel) {
        chatPanel.dispose();
    }
}
exports.deactivate = deactivate;
class ChatPanel {
    constructor(panel, context, oauthManager) {
        this.chatHistory = [];
        this.isBusy = false;
        this.panel = panel;
        this.context = context;
        this.oauthManager = oauthManager;
        this.panel.onDidDispose(() => this.dispose(), null, context.subscriptions);
        this.panel.webview.onDidReceiveMessage(this.handleWebviewMessage.bind(this), null, context.subscriptions);
        this.panel.webview.html = this.getHtml();
        this.postAuthState();
    }
    static createOrShow(context, oauthManager) {
        if (ChatPanel.currentPanel) {
            ChatPanel.currentPanel.panel.reveal(vscode.ViewColumn.Beside);
            return ChatPanel.currentPanel;
        }
        const panel = vscode.window.createWebviewPanel('xipherAiChat', 'Xipher AI', vscode.ViewColumn.Beside, {
            enableScripts: true,
            retainContextWhenHidden: true
        });
        ChatPanel.currentPanel = new ChatPanel(panel, context, oauthManager);
        return ChatPanel.currentPanel;
    }
    dispose() {
        ChatPanel.currentPanel = undefined;
        this.panel.dispose();
    }
    async sendUserMessage(text, displayInWebview) {
        const trimmed = text.trim();
        if (!trimmed) {
            return;
        }
        this.chatHistory.push({ role: 'user', content: trimmed });
        if (displayInWebview) {
            this.postMessage({ type: 'userMessage', text: trimmed });
        }
        await this.runChatCompletion();
    }
    async postAuthState() {
        const useOAuth = vscode.workspace.getConfiguration('xipherAi.openai').get('useOAuth') ?? false;
        const hasOauthToken = await this.oauthManager.hasToken();
        const configKey = vscode.workspace.getConfiguration('xipherAi.openai').get('apiKey') || '';
        const storedKey = await this.context.secrets.get(OPENAI_KEY_SECRET);
        const hasApiKey = Boolean(configKey.trim() || (storedKey && storedKey.trim()));
        this.postMessage({
            type: 'authState',
            useOAuth,
            hasOauthToken,
            hasApiKey
        });
    }
    async handleWebviewMessage(message) {
        if (!message || typeof message !== 'object') {
            return;
        }
        switch (message.type) {
            case 'userMessage':
                await this.sendUserMessage(String(message.text || ''), false);
                break;
            case 'signIn':
                await vscode.commands.executeCommand('xipherAi.signInChatGpt');
                break;
            case 'signOut':
                await vscode.commands.executeCommand('xipherAi.signOutChatGpt');
                break;
            case 'setApiKey':
                await vscode.commands.executeCommand('xipherAi.setOpenAiKey');
                break;
            case 'clearChat':
                this.chatHistory = [];
                this.postMessage({ type: 'clearChat' });
                break;
            default:
                break;
        }
    }
    async runChatCompletion() {
        if (this.isBusy) {
            this.postMessage({ type: 'error', text: 'Already running a request. Please wait.' });
            return;
        }
        this.isBusy = true;
        this.postMessage({ type: 'assistantTyping', active: true });
        try {
            const config = vscode.workspace.getConfiguration('xipherAi');
            const systemPrompt = config.get('systemPrompt') || '';
            const timeoutMs = config.get('requestTimeoutMs') || 45000;
            const openAiConfig = vscode.workspace.getConfiguration('xipherAi.openai');
            const apiBaseUrl = openAiConfig.get('apiBaseUrl') || 'https://api.openai.com/v1';
            const model = openAiConfig.get('model') || 'gpt-4o-mini';
            const useOAuth = openAiConfig.get('useOAuth') ?? false;
            const messages = [];
            if (systemPrompt.trim()) {
                messages.push({ role: 'system', content: systemPrompt.trim() });
            }
            messages.push(...this.chatHistory);
            const token = await this.getAccessToken(useOAuth);
            if (!token) {
                this.postMessage({ type: 'error', text: useOAuth ? 'Sign in with OAuth first.' : 'OpenAI API key is missing.' });
                return;
            }
            const response = await (0, openaiClient_1.createChatCompletion)({
                apiBaseUrl,
                token,
                model,
                messages,
                timeoutMs
            });
            this.chatHistory.push({ role: 'assistant', content: response });
            this.postMessage({ type: 'assistantMessage', text: response });
        }
        catch (error) {
            const message = error instanceof Error ? error.message : 'Unknown error';
            this.postMessage({ type: 'error', text: message });
        }
        finally {
            this.isBusy = false;
            this.postMessage({ type: 'assistantTyping', active: false });
        }
    }
    async getAccessToken(useOAuth) {
        if (useOAuth) {
            return await this.oauthManager.getAccessToken();
        }
        const configKey = vscode.workspace.getConfiguration('xipherAi.openai').get('apiKey') || '';
        if (configKey.trim()) {
            return configKey.trim();
        }
        const storedKey = await this.context.secrets.get(OPENAI_KEY_SECRET);
        return storedKey?.trim();
    }
    getHtml() {
        const webview = this.panel.webview;
        const scriptUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, 'media', 'chat.js'));
        const styleUri = webview.asWebviewUri(vscode.Uri.joinPath(this.context.extensionUri, 'media', 'chat.css'));
        const nonce = getNonce();
        return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource}; script-src 'nonce-${nonce}';">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <link rel="stylesheet" href="${styleUri}">
  <title>Xipher AI</title>
</head>
<body>
  <header class="chat-header">
    <div>
      <div class="title">Xipher AI</div>
      <div class="subtitle" id="authStatus">Not connected</div>
    </div>
    <div class="actions">
      <button class="ghost" id="oauthBtn">OAuth</button>
      <button class="ghost" id="apiKeyBtn">API Key</button>
      <button class="ghost" id="signOutBtn">Sign out</button>
      <button class="ghost" id="clearChatBtn">Clear</button>
    </div>
  </header>
  <main id="messages" class="messages"></main>
  <div class="typing" id="typing">Assistant is typing...</div>
  <footer class="composer">
    <textarea id="prompt" rows="2" placeholder="Ask anything..."></textarea>
    <button id="sendBtn">Send</button>
  </footer>
  <script nonce="${nonce}" src="${scriptUri}"></script>
</body>
</html>`;
    }
    postMessage(message) {
        this.panel.webview.postMessage(message);
    }
}
function getNonce() {
    const possible = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    let text = '';
    for (let i = 0; i < 32; i++) {
        text += possible.charAt(Math.floor(Math.random() * possible.length));
    }
    return text;
}
//# sourceMappingURL=extension.js.map