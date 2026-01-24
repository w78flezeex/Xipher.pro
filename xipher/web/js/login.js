const API_BASE = '';
const SESSION_TOKEN_PLACEHOLDER = 'cookie';

function clearSession() {
    localStorage.removeItem('xipher_token');
    localStorage.removeItem('xipher_user_id');
    localStorage.removeItem('xipher_username');
    try {
        fetch(API_BASE + '/api/logout', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({})
        });
    } catch (_) {}
}

// Проверка, не авторизован ли уже пользователь
document.addEventListener('DOMContentLoaded', () => {
    fetch(API_BASE + '/api/validate-token', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success && data.data) {
            localStorage.setItem('xipher_token', SESSION_TOKEN_PLACEHOLDER);
            if (data.data.user_id) {
                localStorage.setItem('xipher_user_id', data.data.user_id);
            }
            if (data.data.username) {
                localStorage.setItem('xipher_username', data.data.username);
            }
            window.location.href = '/chat';
        } else {
            clearSession();
        }
    })
    .catch(() => {
        // Ошибка проверки - оставляем на странице логина
    });
});

document.getElementById('loginForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    
    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value;
    
    // Clear previous errors
    document.querySelectorAll('.form-error').forEach(el => {
        el.classList.remove('show');
        el.textContent = '';
    });
    
    // Validate
    if (!username) {
        showError('usernameError', 'Введите имя пользователя');
        return;
    }
    
    if (!password) {
        showError('passwordError', 'Введите пароль');
        return;
    }
    
    // Submit
    try {
        const response = await fetch(API_BASE + '/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
        });
        
        const data = await response.json();
        
        if (data.success) {
            if (data.data) {
                localStorage.setItem('xipher_token', SESSION_TOKEN_PLACEHOLDER);
                localStorage.setItem('xipher_user_id', data.data.user_id || '');
                localStorage.setItem('xipher_username', data.data.username || '');
            }
            
            // Show success notification
            if (typeof notifications !== 'undefined') {
                notifications.success('Вход выполнен успешно!');
            }
            
            // Redirect to chat
            setTimeout(() => {
                window.location.href = '/chat';
            }, 1000);
        } else {
            showError('passwordError', data.message || 'Неверное имя пользователя или пароль');
        }
    } catch (error) {
        console.error('Login error:', error);
        showError('passwordError', 'Ошибка соединения с сервером');
    }
});

function showError(elementId, message) {
    const element = document.getElementById(elementId);
    element.textContent = message;
    element.classList.add('show');
}
