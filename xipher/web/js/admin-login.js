(() => {
    const form = document.getElementById('adminLoginForm');
    const errorEl = document.getElementById('loginError');
    const submitBtn = document.getElementById('loginSubmit');

    const showError = (message) => {
        errorEl.textContent = message;
        errorEl.classList.add('show');
    };

    const clearError = () => {
        errorEl.textContent = '';
        errorEl.classList.remove('show');
    };

    const apiPost = async (url, payload) => {
        const res = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'include',
            body: JSON.stringify(payload)
        });
        const text = await res.text();
        try {
            return JSON.parse(text);
        } catch (e) {
            return { success: false, message: text };
        }
    };

    const redirectToPanel = () => {
        window.location.href = '/admin/panel';
    };

    const checkSession = async () => {
        const res = await apiPost('/api/admin/action', { action: 'stats' });
        if (res.success) {
            redirectToPanel();
        }
    };

    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        clearError();
        submitBtn.disabled = true;

        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;

        if (!password) {
            showError('Введите пароль');
            submitBtn.disabled = false;
            return;
        }

        const res = await apiPost('/api/admin/login', { username, password });
        if (res.success) {
            redirectToPanel();
            return;
        }

        showError(res.message || 'Неверные данные');
        submitBtn.disabled = false;
    });

    checkSession();
})();
