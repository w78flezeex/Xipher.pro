(function () {
    const config = {
        apiKey: 'AIzaSyDHirIQepkOOvzxBJzZRvkEKhyg39pwS_M',
        authDomain: 'xipher-21dd8.firebaseapp.com',
        projectId: 'xipher-21dd8',
        storageBucket: 'xipher-21dd8.firebasestorage.app',
        messagingSenderId: '370867742940',
        appId: '1:370867742940:web:49ae7bfbd32d8554bcac19'
    };

    const vapidKey = 'BNrimGePqCbtlzWHUWWSlv_oZJi2IU7tYtGXCsOBzQPhHXiX-Vv6eaMxgR69D-Rb7aer7Zv_h3WXX65lIK8UOPI';

    if (typeof window !== 'undefined') {
        window.FIREBASE_CONFIG = config;
        window.FIREBASE_VAPID_KEY = vapidKey;
    }
    if (typeof self !== 'undefined') {
        self.FIREBASE_CONFIG = config;
        self.FIREBASE_VAPID_KEY = vapidKey;
    }
})();
