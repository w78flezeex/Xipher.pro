importScripts('/js/firebase-config.js');
importScripts('https://www.gstatic.com/firebasejs/9.22.2/firebase-app-compat.js');
importScripts('https://www.gstatic.com/firebasejs/9.22.2/firebase-messaging-compat.js');

if (!self.FIREBASE_CONFIG || !self.FIREBASE_CONFIG.apiKey) {
    console.warn('[Push] Firebase config missing in service worker');
}

try {
    if (self.FIREBASE_CONFIG && self.FIREBASE_CONFIG.apiKey) {
        firebase.initializeApp(self.FIREBASE_CONFIG);
    }
} catch (error) {
    console.warn('[Push] Firebase init failed in service worker:', error);
}

let messaging = null;
try {
    messaging = firebase.messaging();
} catch (error) {
    console.warn('[Push] Firebase messaging init failed in service worker:', error);
}

function buildNotification(payload) {
    const data = payload && payload.data ? payload.data : {};
    const title = data.title || payload?.notification?.title || 'New message';
    const body = data.body || payload?.notification?.body || '';
    const chatId = data.chat_id || data.chatId || '';
    const chatType = data.chat_type || data.chatType || '';
    const senderName = data.sender_name || data.user_name || '';
    const urlPath = chatId ? `/chat/${encodeURIComponent(chatId)}` : '/chat';
    const options = {
        body,
        icon: '/images/xipher-avatar.png',
        data: {
            chatId,
            chatType,
            senderName,
            url: urlPath
        }
    };
    if (chatId) {
        options.tag = `chat:${chatId}`;
    }
    return self.registration.showNotification(title, options);
}

function handleBackgroundMessage(payload) {
    return buildNotification(payload);
}

if (messaging?.onBackgroundMessage) {
    messaging.onBackgroundMessage(handleBackgroundMessage);
} else if (messaging?.setBackgroundMessageHandler) {
    messaging.setBackgroundMessageHandler(handleBackgroundMessage);
}

self.addEventListener('notificationclick', (event) => {
    event.notification.close();
    const data = event.notification.data || {};
    const targetUrl = data.url || '/chat';
    const urlToOpen = new URL(targetUrl, self.location.origin).href;

    event.waitUntil(
        clients.matchAll({ type: 'window', includeUncontrolled: true }).then((clientList) => {
            for (const client of clientList) {
                if (!client?.url || !client.focus) continue;
                if (client.url === urlToOpen || client.url.includes('/chat')) {
                    client.postMessage({ type: 'open_chat', chatId: data.chatId, chatType: data.chatType });
                    return client.focus();
                }
                if ('navigate' in client) {
                    return client.navigate(urlToOpen).then((newClient) => {
                        if (newClient && newClient.focus) {
                            newClient.focus();
                        }
                    });
                }
            }
            return clients.openWindow(urlToOpen);
        })
    );
});
