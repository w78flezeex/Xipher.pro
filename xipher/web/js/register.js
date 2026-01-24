const API_BASE = '';

let usernameCheckTimeout = null;

document.getElementById('username').addEventListener('input', function() {
    const username = this.value.trim();
    const usernameError = document.getElementById('usernameError');
    const usernameSuccess = document.getElementById('usernameSuccess');
    
    // Clear previous timeout
    if (usernameCheckTimeout) {
        clearTimeout(usernameCheckTimeout);
    }
    
    // Validate format
    if (username.length > 0 && !/^[a-zA-Z0-9_]+$/.test(username)) {
        usernameError.textContent = 'Имя пользователя может содержать только буквы, цифры и подчеркивания';
        usernameError.classList.add('show');
        usernameSuccess.classList.remove('show');
        return;
    }
    
    if (username.length > 0 && username.length < 3) {
        usernameError.textContent = 'Имя пользователя должно содержать минимум 3 символа';
        usernameError.classList.add('show');
        usernameSuccess.classList.remove('show');
        return;
    }
    
    if (username.length > 50) {
        usernameError.textContent = 'Имя пользователя не должно превышать 50 символов';
        usernameError.classList.add('show');
        usernameSuccess.classList.remove('show');
        return;
    }
    
    // Check availability after delay
    if (username.length >= 3) {
        usernameCheckTimeout = setTimeout(() => {
            checkUsername(username);
        }, 500);
    } else {
        usernameError.classList.remove('show');
        usernameSuccess.classList.remove('show');
    }
});

async function checkUsername(username) {
    const usernameError = document.getElementById('usernameError');
    const usernameSuccess = document.getElementById('usernameSuccess');
    
    try {
        const response = await fetch(API_BASE + '/api/check-username', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username })
        });
        
        const data = await response.json();
        
        if (data.success) {
            usernameError.classList.remove('show');
            usernameSuccess.textContent = 'Имя пользователя доступно';
            usernameSuccess.classList.add('show');
        } else {
            usernameError.textContent = data.message || 'Имя пользователя уже занято';
            usernameError.classList.add('show');
            usernameSuccess.classList.remove('show');
        }
    } catch (error) {
        console.error('Error checking username:', error);
    }
}

document.getElementById('registerForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    
    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value;
    const confirmPassword = document.getElementById('confirmPassword').value;
    
    // Clear previous errors
    document.querySelectorAll('.form-error').forEach(el => {
        el.classList.remove('show');
        el.textContent = '';
    });
    document.querySelectorAll('.form-success').forEach(el => {
        el.classList.remove('show');
    });
    
    // Validate
    let hasErrors = false;
    
    if (username.length < 3 || username.length > 50) {
        showError('usernameError', 'Имя пользователя должно содержать от 3 до 50 символов');
        hasErrors = true;
    }
    
    if (!/^[a-zA-Z0-9_]+$/.test(username)) {
        showError('usernameError', 'Имя пользователя может содержать только буквы, цифры и подчеркивания');
        hasErrors = true;
    }
    
    if (password.length < 6) {
        showError('passwordError', 'Пароль должен содержать минимум 6 символов');
        hasErrors = true;
    }
    
    if (password !== confirmPassword) {
        showError('confirmPasswordError', 'Пароли не совпадают');
        hasErrors = true;
    }
    
    if (hasErrors) {
        return;
    }
    
    // Submit
    try {
        const response = await fetch(API_BASE + '/api/register', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
        });
        
        const data = await response.json();
        
        if (data.success) {
            if (typeof notifications !== 'undefined') {
                notifications.success('Регистрация успешна! Перенаправление на страницу входа...');
            } else {
                alert('Регистрация успешна! Теперь вы можете войти.');
            }
            setTimeout(() => {
                window.location.href = '/login';
            }, 1500);
        } else {
            showError('usernameError', data.message || 'Ошибка при регистрации');
        }
    } catch (error) {
        console.error('Registration error:', error);
        showError('usernameError', 'Ошибка соединения с сервером');
    }
});

function showError(elementId, message) {
    const element = document.getElementById(elementId);
    element.textContent = message;
    element.classList.add('show');
}
