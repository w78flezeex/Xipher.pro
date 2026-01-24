var dictionary = {
    en: {
        // Menu
        "menu.file": "File",
        "menu.settings": "Settings",
        "menu.quit": "Quit",
        "menu.search": "Search in chat",
        "menu.pin": "Pin chat",
        "menu.mute": "Mute notifications",
        "menu.clear": "Clear history",
        "menu.reply": "Reply",
        "menu.copy": "Copy",
        "menu.delete": "Delete",

        // Auth
        "auth.title": "Sign in",
        "auth.subtitle": "Secure messaging for everyone",
        "auth.username": "Enter your username",
        "auth.usernameLabel": "Username",
        "auth.password": "Enter your password",
        "auth.passwordLabel": "Password",
        "auth.loading": "Signing in...",
        "auth.login": "Sign in",
        "auth.register": "Create account",
        "auth.or": "or",
        "auth.restoring": "Restoring session...",
        "auth.settings": "Connection settings",
        "auth.error.missing_credentials": "Username and password are required",
        "auth.error.network": "Cannot reach server. Check connection settings.",
        "auth.error.invalid_base_url": "Invalid base URL. Check connection settings.",
        "auth.error.invalid_response": "Invalid server response.",
        "auth.error.login_failed": "Login failed.",

        // Search
        "search.placeholder": "Search chats...",

        // Tabs
        "tabs.all": "All",
        "tabs.personal": "Personal",
        "tabs.groups": "Groups",
        "tabs.channels": "Channels",

        // Status
        "status.online": "Online",
        "status.offline": "Offline",
        "status.lastSeen": "Last seen recently",
        "status.typing": "typing",

        // Chats
        "chats.empty": "No chats yet",
        "chat.new": "New chat",
        "chat.select": "Select a chat",
        "chat.selectPrompt": "Select a chat to start messaging",
        "chat.selectHint": "Choose a conversation from the list on the left or start a new chat",
        "chat.loading": "Loading messages...",
        "chat.empty": "No messages yet",
        "chat.noMessages": "No messages yet. Start the conversation!",
        "chat.compose": "Write a message...",
        "chat.attach": "Attach file",
        "chat.send": "Send",
        "chat.voice": "Voice message",
        "chat.emoji": "Emoji",
        "chat.dropFiles": "Drop files here to send",
        "chat.replyTo": "Reply to",

        // Banners
        "banner.offline": "You're offline. Reconnecting...",
        "banner.serverError": "Server error. Please try again later.",

        // Info panel
        "info.title": "Chat Info",
        "info.placeholder": "Selected chat details will appear here.",
        "info.settings": "App Settings",
        "info.notifications": "Notifications",
        "info.pin": "Pin to top",
        "info.search": "Search in chat",
        "info.media": "Shared media",
        "info.sharedMedia": "Shared Media",
        "info.noMedia": "No shared media yet",
        "info.clearHistory": "Clear history",
        "info.deleteChat": "Delete chat",
        "info.links": "Shared links",
        "info.files": "Files",

        // Settings
        "settings.title": "Settings",
        "settings.theme": "Theme",
        "settings.language": "Language",
        "settings.close": "Close",
        "settings.logout": "Sign out",
        "settings.save": "Save",
        "settings.editProfile": "Edit profile",
        
        // Settings Navigation
        "settings.nav.account": "My Account",
        "settings.nav.notifications": "Notifications",
        "settings.nav.calls": "Calls",
        "settings.nav.privacy": "Privacy & Security",
        "settings.nav.sessions": "Active Sessions",
        "settings.nav.blocked": "Blocked Users",
        "settings.nav.language": "Language",
        "settings.nav.premium": "Xipher Premium",
        "settings.nav.faq": "Xipher FAQ",

        // Settings - Account
        "settings.account.profile": "Profile",
        "settings.account.photoEdit": "Edit photo",
        "settings.account.changePhoto": "Change photo",
        "settings.account.basic": "Basic Info",
        "settings.account.firstName": "First Name",
        "settings.account.lastName": "Last Name",
        "settings.account.username": "Username",
        "settings.account.usernameNote": "Username change will be available later",

        // Settings - Notifications
        "settings.notifications.title": "Notifications",
        "settings.notifications.desktop": "Desktop notifications",
        "settings.notifications.sound": "Message sounds",
        "settings.notifications.preview": "Show message preview",
        "settings.notifications.callSound": "Call sounds",

        // Settings - Calls
        "settings.calls.title": "Calls",
        "settings.calls.accept": "Accept calls",
        "settings.calls.who": "Who can call",

        // Settings - Privacy
        "settings.privacy.autoDelete": "Auto-delete messages",
        "settings.privacy.interval": "Interval",

        // Settings - Sessions
        "settings.sessions.title": "Active Sessions",
        "settings.sessions.terminateAll": "Terminate all other sessions",

        // Settings - Blocked
        "settings.blocked.title": "Blocked Users",
        "settings.blocked.empty": "List is empty",

        // Settings - Language
        "settings.language.title": "Language & Theme",
        "settings.language.select": "Language",

        // Settings - Premium
        "settings.premium.description": "Get more features with Premium subscription",
        "settings.premium.upgrade": "Upgrade to Premium",

        // Settings - FAQ
        "settings.faq.description": "Frequently Asked Questions",
        "settings.faq.open": "Open FAQ",

        // Dialogs
        "dialog.newChat.title": "New Chat",
        "dialog.newChat.body": "Start a new conversation by searching for users.",
        "dialog.newChat.findUser": "Find User",
        "dialog.newChat.createGroup": "Create Group",
        "dialog.newChat.usernameHint": "Enter the user's username",
        "dialog.newChat.search": "Find",
        "dialog.newChat.searching": "Searching...",
        "dialog.newChat.userFound": "User found",
        "dialog.newChat.notFound": "User not found",
        "dialog.newChat.startChat": "Start Chat",
        "dialog.createGroup.title": "Create Group",
        "dialog.createGroup.nameHint": "Group name",
        "dialog.createGroup.namePlaceholder": "Enter name...",
        "dialog.createGroup.membersHint": "Add members",
        "dialog.createGroup.selectFromChats": "Select from existing chats",
        "dialog.createGroup.create": "Create Group",
        "dialog.profile.title": "Profile",
        "dialog.profile.body": "View and edit your profile information.",
        "dialog.search.title": "Search in Chat",
        "dialog.search.body": "Search for messages in this conversation.",
        "dialog.clearHistory.title": "Clear History",
        "dialog.clearHistory.body": "Are you sure you want to clear all messages? This cannot be undone.",
        "dialog.deleteChat.title": "Delete Chat",
        "dialog.deleteChat.body": "Are you sure you want to delete this chat? This cannot be undone.",

        // Emoji
        "emoji.title": "Emoji",
        "emoji.search": "Search emoji...",

        // Toasts
        "toast.copied": "Copied to clipboard",
        "toast.notImplemented": "Feature coming soon",
        "toast.voiceNotSupported": "Voice messages coming soon",
        "toast.messageSent": "Message sent",
        "toast.messageDeleted": "Message deleted",

        // Calls
        "call.title": "Call",
        "call.ready": "Ready to call",
        "call.connecting": "Connecting...",
        "call.ringing": "Ringing...",
        "call.inCall": "In call",
        "call.close": "Close",
        "call.mute": "Mute",
        "call.video": "Video"
    },
    ru: {
        // Меню
        "menu.file": "Файл",
        "menu.settings": "Настройки",
        "menu.quit": "Выход",
        "menu.search": "Поиск в чате",
        "menu.pin": "Закрепить чат",
        "menu.mute": "Отключить уведомления",
        "menu.clear": "Очистить историю",
        "menu.reply": "Ответить",
        "menu.copy": "Копировать",
        "menu.delete": "Удалить",

        // Авторизация
        "auth.title": "Вход",
        "auth.subtitle": "Безопасный мессенджер для всех",
        "auth.username": "Введите имя пользователя",
        "auth.usernameLabel": "Имя пользователя",
        "auth.password": "Введите пароль",
        "auth.passwordLabel": "Пароль",
        "auth.loading": "Выполняется вход...",
        "auth.login": "Войти",
        "auth.register": "Создать аккаунт",
        "auth.or": "или",
        "auth.restoring": "Восстановление сессии...",
        "auth.settings": "Настройки подключения",
        "auth.error.missing_credentials": "Введите имя пользователя и пароль",
        "auth.error.network": "Не удалось подключиться к серверу. Проверьте настройки подключения.",
        "auth.error.invalid_base_url": "Неверный базовый URL. Проверьте настройки подключения.",
        "auth.error.invalid_response": "Некорректный ответ сервера.",
        "auth.error.login_failed": "Не удалось войти.",

        // Поиск
        "search.placeholder": "Поиск чатов...",

        // Вкладки
        "tabs.all": "Все",
        "tabs.personal": "Личные",
        "tabs.groups": "Группы",
        "tabs.channels": "Каналы",

        // Статус
        "status.online": "В сети",
        "status.offline": "Не в сети",
        "status.lastSeen": "Был(а) недавно",
        "status.typing": "печатает",

        // Чаты
        "chats.empty": "Чатов пока нет",
        "chat.new": "Новый чат",
        "chat.select": "Выберите чат",
        "chat.selectPrompt": "Выберите чат для начала общения",
        "chat.selectHint": "Выберите беседу из списка слева или начните новый чат",
        "chat.loading": "Загрузка сообщений...",
        "chat.empty": "Сообщений пока нет",
        "chat.noMessages": "Сообщений пока нет. Начните общение!",
        "chat.compose": "Введите сообщение...",
        "chat.attach": "Прикрепить файл",
        "chat.send": "Отправить",
        "chat.voice": "Голосовое сообщение",
        "chat.emoji": "Эмодзи",
        "chat.dropFiles": "Перетащите файлы сюда",
        "chat.replyTo": "Ответ",

        // Баннеры
        "banner.offline": "Нет подключения. Переподключение...",
        "banner.serverError": "Ошибка сервера. Попробуйте позже.",

        // Панель информации
        "info.title": "Информация о чате",
        "info.placeholder": "Здесь появятся детали выбранного чата.",
        "info.settings": "Настройки приложения",
        "info.notifications": "Уведомления",
        "info.pin": "Закрепить",
        "info.search": "Поиск в чате",
        "info.media": "Медиафайлы",
        "info.sharedMedia": "Общие медиафайлы",
        "info.noMedia": "Нет медиафайлов",
        "info.clearHistory": "Очистить историю",
        "info.deleteChat": "Удалить чат",
        "info.links": "Ссылки",
        "info.files": "Файлы",

        // Настройки
        "settings.title": "Настройки",
        "settings.theme": "Тема",
        "settings.language": "Язык",
        "settings.close": "Закрыть",
        "settings.logout": "Выйти",
        "settings.save": "Сохранить",
        "settings.editProfile": "Редактировать профиль",

        // Навигация настроек
        "settings.nav.account": "Мой аккаунт",
        "settings.nav.notifications": "Уведомления и звуки",
        "settings.nav.calls": "Звонки",
        "settings.nav.privacy": "Приватность и безопасность",
        "settings.nav.sessions": "Активные сеансы",
        "settings.nav.blocked": "Заблокированные",
        "settings.nav.language": "Язык",
        "settings.nav.premium": "Xipher Premium",
        "settings.nav.faq": "Xipher FAQ",

        // Настройки - Аккаунт
        "settings.account.profile": "Профиль",
        "settings.account.photoEdit": "Редактирование фото",
        "settings.account.changePhoto": "Сменить фото",
        "settings.account.basic": "Основные данные",
        "settings.account.firstName": "Имя",
        "settings.account.lastName": "Фамилия",
        "settings.account.username": "Username",
        "settings.account.usernameNote": "Смена username будет доступна позже",

        // Настройки - Уведомления
        "settings.notifications.title": "Уведомления",
        "settings.notifications.desktop": "Уведомления рабочего стола",
        "settings.notifications.sound": "Звук сообщений",
        "settings.notifications.preview": "Показывать превью сообщений",
        "settings.notifications.callSound": "Звук звонков",

        // Настройки - Звонки
        "settings.calls.title": "Звонки",
        "settings.calls.accept": "Принимать звонки",
        "settings.calls.who": "Кто может звонить",

        // Настройки - Приватность
        "settings.privacy.autoDelete": "Авто-удаление сообщений",
        "settings.privacy.interval": "Интервал",

        // Настройки - Сеансы
        "settings.sessions.title": "Активные сеансы",
        "settings.sessions.terminateAll": "Завершить все другие сеансы",

        // Настройки - Заблокированные
        "settings.blocked.title": "Заблокированные",
        "settings.blocked.empty": "Список пуст",

        // Настройки - Язык
        "settings.language.title": "Язык и тема",
        "settings.language.select": "Язык",

        // Настройки - Premium
        "settings.premium.description": "Получите больше возможностей с Premium подпиской",
        "settings.premium.upgrade": "Улучшить до Premium",

        // Настройки - FAQ
        "settings.faq.description": "Часто задаваемые вопросы",
        "settings.faq.open": "Открыть FAQ",

        // Диалоги
        "dialog.newChat.title": "Новый чат",
        "dialog.newChat.body": "Начните новую беседу, найдя пользователей.",
        "dialog.newChat.findUser": "Найти пользователя",
        "dialog.newChat.createGroup": "Создать группу",
        "dialog.newChat.usernameHint": "Введите username пользователя",
        "dialog.newChat.search": "Найти",
        "dialog.newChat.searching": "Поиск...",
        "dialog.newChat.userFound": "Пользователь найден",
        "dialog.newChat.notFound": "Пользователь не найден",
        "dialog.newChat.startChat": "Начать чат",
        "dialog.createGroup.title": "Создать группу",
        "dialog.createGroup.nameHint": "Название группы",
        "dialog.createGroup.namePlaceholder": "Введите название...",
        "dialog.createGroup.membersHint": "Добавьте участников",
        "dialog.createGroup.selectFromChats": "Выберите из существующих чатов",
        "dialog.createGroup.create": "Создать группу",
        "dialog.profile.title": "Профиль",
        "dialog.profile.body": "Просмотр и редактирование профиля.",
        "dialog.search.title": "Поиск в чате",
        "dialog.search.body": "Поиск сообщений в этом чате.",
        "dialog.clearHistory.title": "Очистить историю",
        "dialog.clearHistory.body": "Вы уверены, что хотите очистить все сообщения? Это действие нельзя отменить.",
        "dialog.deleteChat.title": "Удалить чат",
        "dialog.deleteChat.body": "Вы уверены, что хотите удалить этот чат? Это действие нельзя отменить.",

        // Эмодзи
        "emoji.title": "Эмодзи",
        "emoji.search": "Поиск эмодзи...",

        // Уведомления
        "toast.copied": "Скопировано в буфер обмена",
        "toast.notImplemented": "Функция скоро будет доступна",
        "toast.voiceNotSupported": "Голосовые сообщения скоро будут доступны",
        "toast.messageSent": "Сообщение отправлено",
        "toast.messageDeleted": "Сообщение удалено",

        // Звонки
        "call.title": "Звонок",
        "call.ready": "Готов к звонку",
        "call.connecting": "Соединение...",
        "call.ringing": "Вызов...",
        "call.inCall": "Идёт звонок",
        "call.close": "Закрыть",
        "call.mute": "Выключить микрофон",
        "call.video": "Видео"
    }
};

function t(key, lang) {
    var langDict = dictionary[lang] || dictionary.en;
    if (langDict && langDict[key]) {
        return langDict[key];
    }
    return dictionary.en[key] || key;
}
