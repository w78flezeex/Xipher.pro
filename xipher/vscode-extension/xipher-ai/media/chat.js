(function () {
  const vscode = acquireVsCodeApi();
  const messagesEl = document.getElementById('messages');
  const promptEl = document.getElementById('prompt');
  const sendBtn = document.getElementById('sendBtn');
  const authStatusEl = document.getElementById('authStatus');
  const oauthBtn = document.getElementById('oauthBtn');
  const apiKeyBtn = document.getElementById('apiKeyBtn');
  const signOutBtn = document.getElementById('signOutBtn');
  const clearBtn = document.getElementById('clearChatBtn');
  const typingEl = document.getElementById('typing');

  function addMessage(text, role) {
    const div = document.createElement('div');
    div.className = `message ${role}`;
    div.textContent = text;
    messagesEl.appendChild(div);
    messagesEl.scrollTop = messagesEl.scrollHeight;
  }

  function sendPrompt() {
    const text = promptEl.value.trim();
    if (!text) {
      return;
    }
    addMessage(text, 'user');
    promptEl.value = '';
    vscode.postMessage({ type: 'userMessage', text });
  }

  sendBtn.addEventListener('click', sendPrompt);
  promptEl.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault();
      sendPrompt();
    }
  });

  oauthBtn.addEventListener('click', () => vscode.postMessage({ type: 'signIn' }));
  apiKeyBtn.addEventListener('click', () => vscode.postMessage({ type: 'setApiKey' }));
  signOutBtn.addEventListener('click', () => vscode.postMessage({ type: 'signOut' }));
  clearBtn.addEventListener('click', () => vscode.postMessage({ type: 'clearChat' }));

  window.addEventListener('message', (event) => {
    const message = event.data;
    if (!message || typeof message !== 'object') {
      return;
    }

    switch (message.type) {
      case 'assistantMessage':
        addMessage(message.text || '', 'assistant');
        break;
      case 'userMessage':
        addMessage(message.text || '', 'user');
        break;
      case 'error':
        addMessage(message.text || 'Unknown error', 'error');
        break;
      case 'assistantTyping':
        typingEl.style.display = message.active ? 'block' : 'none';
        break;
      case 'clearChat':
        messagesEl.innerHTML = '';
        break;
      case 'authState':
        updateAuthStatus(message);
        break;
      default:
        break;
    }
  });

  function updateAuthStatus(state) {
    const useOAuth = !!state.useOAuth;
    const hasOauthToken = !!state.hasOauthToken;
    const hasApiKey = !!state.hasApiKey;

    if (useOAuth) {
      authStatusEl.textContent = hasOauthToken ? 'OAuth connected' : 'OAuth not connected';
    } else {
      authStatusEl.textContent = hasApiKey ? 'API key ready' : 'API key missing';
    }
  }
})();
