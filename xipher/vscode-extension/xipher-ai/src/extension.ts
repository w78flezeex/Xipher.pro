import * as vscode from 'vscode';
import { createChatCompletion, ChatMessage } from './openaiClient';
import { AuthUriHandler, OAuthManager } from './oauth';

const OPENAI_KEY_SECRET = 'xipherAi.openai.apiKey';

let chatPanel: ChatPanel | undefined;

export function activate(context: vscode.ExtensionContext) {
  const uriHandler = new AuthUriHandler();
  context.subscriptions.push(vscode.window.registerUriHandler(uriHandler));

  const oauthManager = new OAuthManager(context, uriHandler);

  context.subscriptions.push(
    vscode.commands.registerCommand('xipherAi.openChat', () => {
      chatPanel = ChatPanel.createOrShow(context, oauthManager);
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('xipherAi.signInChatGpt', async () => {
      await oauthManager.signIn();
      chatPanel?.postAuthState();
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('xipherAi.signOutChatGpt', async () => {
      await oauthManager.signOut();
      chatPanel?.postAuthState();
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('xipherAi.setOpenAiKey', async () => {
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
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('xipherAi.explainSelection', async () => {
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
    })
  );
}

export function deactivate() {
  if (chatPanel) {
    chatPanel.dispose();
  }
}

class ChatPanel {
  public static currentPanel: ChatPanel | undefined;
  private readonly panel: vscode.WebviewPanel;
  private readonly context: vscode.ExtensionContext;
  private readonly oauthManager: OAuthManager;
  private chatHistory: ChatMessage[] = [];
  private isBusy = false;

  private constructor(panel: vscode.WebviewPanel, context: vscode.ExtensionContext, oauthManager: OAuthManager) {
    this.panel = panel;
    this.context = context;
    this.oauthManager = oauthManager;

    this.panel.onDidDispose(() => this.dispose(), null, context.subscriptions);
    this.panel.webview.onDidReceiveMessage(this.handleWebviewMessage.bind(this), null, context.subscriptions);

    this.panel.webview.html = this.getHtml();
    this.postAuthState();
  }

  public static createOrShow(context: vscode.ExtensionContext, oauthManager: OAuthManager): ChatPanel {
    if (ChatPanel.currentPanel) {
      ChatPanel.currentPanel.panel.reveal(vscode.ViewColumn.Beside);
      return ChatPanel.currentPanel;
    }

    const panel = vscode.window.createWebviewPanel(
      'xipherAiChat',
      'Xipher AI',
      vscode.ViewColumn.Beside,
      {
        enableScripts: true,
        retainContextWhenHidden: true
      }
    );

    ChatPanel.currentPanel = new ChatPanel(panel, context, oauthManager);
    return ChatPanel.currentPanel;
  }

  public dispose() {
    ChatPanel.currentPanel = undefined;
    this.panel.dispose();
  }

  public async sendUserMessage(text: string, displayInWebview: boolean) {
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

  public async postAuthState() {
    const useOAuth = vscode.workspace.getConfiguration('xipherAi.openai').get<boolean>('useOAuth') ?? false;
    const hasOauthToken = await this.oauthManager.hasToken();
    const configKey = vscode.workspace.getConfiguration('xipherAi.openai').get<string>('apiKey') || '';
    const storedKey = await this.context.secrets.get(OPENAI_KEY_SECRET);
    const hasApiKey = Boolean(configKey.trim() || (storedKey && storedKey.trim()));

    this.postMessage({
      type: 'authState',
      useOAuth,
      hasOauthToken,
      hasApiKey
    });
  }

  private async handleWebviewMessage(message: any) {
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

  private async runChatCompletion() {
    if (this.isBusy) {
      this.postMessage({ type: 'error', text: 'Already running a request. Please wait.' });
      return;
    }

    this.isBusy = true;
    this.postMessage({ type: 'assistantTyping', active: true });

    try {
      const config = vscode.workspace.getConfiguration('xipherAi');
      const systemPrompt = config.get<string>('systemPrompt') || '';
      const timeoutMs = config.get<number>('requestTimeoutMs') || 45000;
      const openAiConfig = vscode.workspace.getConfiguration('xipherAi.openai');
      const apiBaseUrl = openAiConfig.get<string>('apiBaseUrl') || 'https://api.openai.com/v1';
      const model = openAiConfig.get<string>('model') || 'gpt-4o-mini';
      const useOAuth = openAiConfig.get<boolean>('useOAuth') ?? false;

      const messages: ChatMessage[] = [];
      if (systemPrompt.trim()) {
        messages.push({ role: 'system', content: systemPrompt.trim() });
      }
      messages.push(...this.chatHistory);

      const token = await this.getAccessToken(useOAuth);
      if (!token) {
        this.postMessage({ type: 'error', text: useOAuth ? 'Sign in with OAuth first.' : 'OpenAI API key is missing.' });
        return;
      }

      const response = await createChatCompletion({
        apiBaseUrl,
        token,
        model,
        messages,
        timeoutMs
      });

      this.chatHistory.push({ role: 'assistant', content: response });
      this.postMessage({ type: 'assistantMessage', text: response });
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Unknown error';
      this.postMessage({ type: 'error', text: message });
    } finally {
      this.isBusy = false;
      this.postMessage({ type: 'assistantTyping', active: false });
    }
  }

  private async getAccessToken(useOAuth: boolean): Promise<string | undefined> {
    if (useOAuth) {
      return await this.oauthManager.getAccessToken();
    }

    const configKey = vscode.workspace.getConfiguration('xipherAi.openai').get<string>('apiKey') || '';
    if (configKey.trim()) {
      return configKey.trim();
    }
    const storedKey = await this.context.secrets.get(OPENAI_KEY_SECRET);
    return storedKey?.trim();
  }

  private getHtml(): string {
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

  private postMessage(message: any) {
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
