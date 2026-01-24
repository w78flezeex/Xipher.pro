(function () {
    const TOKEN_STORAGE_KEY = 'xipher_fcm_token';
    let initStarted = false;

    function hasFirebaseConfig(config) {
        return !!(config && typeof config.apiKey === 'string' && config.apiKey.trim());
    }

    async function registerToken(authToken, fcmToken) {
        if (!authToken || !fcmToken) return false;
        try {
            const response = await fetch('/api/register-push-token', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: authToken,
                    device_token: fcmToken,
                    platform: 'web'
                })
            });
            const data = await response.json().catch(() => ({}));
            return response.ok && data.success !== false;
        } catch (error) {
            console.warn('[Push] Token sync failed:', error);
            return false;
        }
    }

    async function init({ authToken } = {}) {
        if (initStarted) return;
        initStarted = true;

        if (!authToken) {
            console.warn('[Push] Missing auth token');
            return;
        }
        if (!('Notification' in window)) {
            console.warn('[Push] Notifications not supported');
            return;
        }
        if (!('serviceWorker' in navigator)) {
            console.warn('[Push] Service workers not supported');
            return;
        }

        const config = window.FIREBASE_CONFIG;
        if (!hasFirebaseConfig(config)) {
            console.warn('[Push] Firebase config missing');
            return;
        }
        if (!window.firebase || typeof window.firebase.initializeApp !== 'function') {
            console.warn('[Push] Firebase SDK missing');
            return;
        }

        try {
            if (!window.firebase.apps || window.firebase.apps.length === 0) {
                window.firebase.initializeApp(config);
            }
        } catch (error) {
            console.warn('[Push] Firebase init failed:', error);
            return;
        }

        let permission = Notification.permission;
        if (permission === 'default') {
            try {
                permission = await Notification.requestPermission();
            } catch (_) {
                permission = 'denied';
            }
        }
        if (permission !== 'granted') {
            console.warn('[Push] Notification permission not granted');
            return;
        }

        let registration;
        try {
            registration = await navigator.serviceWorker.register('/firebase-messaging-sw.js');
        } catch (error) {
            console.warn('[Push] Service worker registration failed:', error);
            return;
        }

        let messaging;
        try {
            messaging = window.firebase.messaging();
        } catch (error) {
            console.warn('[Push] Firebase messaging unavailable:', error);
            return;
        }

        let fcmToken = '';
        try {
            fcmToken = await messaging.getToken({
                vapidKey: window.FIREBASE_VAPID_KEY || undefined,
                serviceWorkerRegistration: registration
            });
        } catch (error) {
            console.warn('[Push] Token fetch failed:', error);
            return;
        }

        if (!fcmToken) {
            console.warn('[Push] Empty token');
            return;
        }

        const cached = localStorage.getItem(TOKEN_STORAGE_KEY);
        if (cached === fcmToken) return;

        const ok = await registerToken(authToken, fcmToken);
        if (ok) {
            localStorage.setItem(TOKEN_STORAGE_KEY, fcmToken);
        }
    }

    window.xipherPush = { init };
})();
