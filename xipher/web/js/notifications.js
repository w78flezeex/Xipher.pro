// Система красивых уведомлений

class NotificationManager {
    constructor() {
        this.container = null;
        this.init();
    }

    init() {
        // Создаем контейнер для уведомлений
        this.container = document.createElement('div');
        this.container.id = 'notification-container';
        this.container.className = 'notification-container';
        document.body.appendChild(this.container);
    }

    show(message, type = 'info', duration = 3000) {
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        const icon = this.getIcon(type);
        const content = document.createElement('div');
        content.className = 'notification-content';

        const iconEl = document.createElement('div');
        iconEl.className = 'notification-icon';
        iconEl.textContent = icon;

        const messageEl = document.createElement('div');
        messageEl.className = 'notification-message';
        messageEl.textContent = String(message ?? '');

        const closeBtn = document.createElement('button');
        closeBtn.className = 'notification-close';
        closeBtn.type = 'button';
        closeBtn.textContent = '×';

        content.appendChild(iconEl);
        content.appendChild(messageEl);
        notification.appendChild(content);
        notification.appendChild(closeBtn);

        this.container.appendChild(notification);

        // Анимация появления
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);

        // Закрытие по клику
        closeBtn.addEventListener('click', () => {
            this.hide(notification);
        });

        // Автоматическое закрытие
        if (duration > 0) {
            setTimeout(() => {
                this.hide(notification);
            }, duration);
        }

        return notification;
    }

    hide(notification) {
        notification.classList.remove('show');
        notification.classList.add('hide');
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 300);
    }

    getIcon(type) {
        const icons = {
            success: '✓',
            error: '✕',
            warning: '⚠',
            info: 'ℹ'
        };
        return icons[type] || icons.info;
    }

    success(message, duration) {
        return this.show(message, 'success', duration);
    }

    error(message, duration) {
        return this.show(message, 'error', duration);
    }

    warning(message, duration) {
        return this.show(message, 'warning', duration);
    }

    info(message, duration) {
        return this.show(message, 'info', duration);
    }
}

// Глобальный экземпляр
const notifications = new NotificationManager();
