const SESSION_TOKEN_PLACEHOLDER = 'cookie';

function setSessionPlaceholder() {
  try {
    const existing = localStorage.getItem('xipher_token');
    if (existing !== SESSION_TOKEN_PLACEHOLDER) {
      localStorage.setItem('xipher_token', SESSION_TOKEN_PLACEHOLDER);
    }
  } catch (_) {}
}

function clearLocalSession() {
  try {
    localStorage.removeItem('xipher_token');
    localStorage.removeItem('xipher_user_id');
    localStorage.removeItem('xipher_username');
  } catch (_) {}
}

async function validateSession() {
  try {
    const res = await fetch('/api/validate-token', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({})
    });
    const data = await res.json();
    return data;
  } catch (_) {
    return null;
  }
}

async function requireSessionOrRedirect({ redirectTo = '/login' } = {}) {
  const data = await validateSession();
  if (data && data.success && data.data) {
    setSessionPlaceholder();
    if (data.data.user_id) {
      try { localStorage.setItem('xipher_user_id', data.data.user_id); } catch (_) {}
    }
    if (data.data.username) {
      try { localStorage.setItem('xipher_username', data.data.username); } catch (_) {}
    }
    return data;
  }
  clearLocalSession();
  if (redirectTo) {
    window.location.href = redirectTo;
  }
  return null;
}

async function logout() {
  try {
    await fetch('/api/logout', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({})
    });
  } catch (_) {}
  clearLocalSession();
}

window.xipherSession = {
  SESSION_TOKEN_PLACEHOLDER,
  setSessionPlaceholder,
  clearLocalSession,
  validateSession,
  requireSessionOrRedirect,
  logout
};

setSessionPlaceholder();
