// Функционал для работы с каналами

let channels = [];
let currentChannel = null;
let currentChannelInfo = null; // Информация о текущем канале (роль, подписчики и т.д.)
let currentChannelMessages = [];
let channelInfoSelectedMessages = new Set();
let channelInviteCache = new Map();
let channelAdminPermModalState = { member: null };
let channelSupportsAdminPerms = false;

const CHANNEL_ADMIN_PERMS = {
    CHANGE_INFO: 1,
    POST_MESSAGES: 2,
    INVITE: 16,
    RESTRICT: 32,
    PIN: 64,
    PROMOTE: 128
};

const CHANNEL_ADMIN_PERM_OPTIONS = [
    { bit: CHANNEL_ADMIN_PERMS.CHANGE_INFO, label: 'Изменение информации', short: 'Инфо', hint: 'Название, описание, аватар' },
    { bit: CHANNEL_ADMIN_PERMS.POST_MESSAGES, label: 'Управление сообщениями', short: 'Сообщения', hint: 'Отправка, редактирование, удаление' },
    { bit: CHANNEL_ADMIN_PERMS.INVITE, label: 'Добавлять участников', short: 'Участники', hint: 'Приглашения и ссылки' },
    { bit: CHANNEL_ADMIN_PERMS.RESTRICT, label: 'Блокировать пользователей', short: 'Блокировки', hint: 'Баны и ограничения' },
    { bit: CHANNEL_ADMIN_PERMS.PIN, label: 'Закреплять сообщения', short: 'Пины', hint: 'Пины и важные' },
    { bit: CHANNEL_ADMIN_PERMS.PROMOTE, label: 'Добавлять админов', short: 'Админы', hint: 'Назначение прав' }
];

const DEFAULT_CHANNEL_ADMIN_PERMS =
    CHANNEL_ADMIN_PERMS.CHANGE_INFO |
    CHANNEL_ADMIN_PERMS.POST_MESSAGES |
    CHANNEL_ADMIN_PERMS.INVITE;

// Создаем заглушку модуля сразу, чтобы избежать ошибок до полной инициализации
// Реальный модуль будет создан в конце файла
if (!window.channelsModule) {
    window.channelsModule = {
        _pending: true,
        _waiting: []
    };
}

function setupCreateChannelModal() {
    const createBtn = document.getElementById('createChannelBtn');
    const modal = document.getElementById('createChannelModal');
    const confirmBtn = document.getElementById('confirmCreateChannelBtn');
    const cancelBtn = document.getElementById('cancelCreateChannelBtn');
    const nameInput = document.getElementById('channelNameInput');
    const descInput = document.getElementById('channelDescriptionInput');

    if (!createBtn) {
        console.error('createChannelBtn not found');
        return;
    }
    
    if (!modal) {
        console.error('createChannelModal not found');
        return;
    }

    console.log('Setting up create channel modal handlers');
    
    createBtn.addEventListener('click', (e) => {
        e.preventDefault();
        e.stopPropagation();
        console.log('Create channel button clicked');
        if (modal) {
            modal.style.display = 'flex';
            if (nameInput) nameInput.value = '';
            if (descInput) descInput.value = '';
        }
    });

    cancelBtn?.addEventListener('click', () => {
        modal.style.display = 'none';
    });

    const closeBtn = document.getElementById('closeCreateChannelModal');
    closeBtn?.addEventListener('click', () => {
        modal.style.display = 'none';
    });

    confirmBtn?.addEventListener('click', async () => {
        const name = nameInput.value.trim();
        const description = descInput.value.trim();
        const customLinkInput = document.getElementById('channelCustomLinkInput');
        const custom_link = customLinkInput ? customLinkInput.value.trim() : '';

        if (!name || name.length < 3) {
            notifications.error('Название канала должно быть не менее 3 символов');
            return;
        }

        if (custom_link && (custom_link.length < 3 || custom_link.length > 50)) {
            notifications.error('Username должен быть от 3 до 50 символов');
            return;
        }

        await createChannel(name, description, custom_link);
        modal.style.display = 'none';
    });

    // Закрытие по клику вне модального окна
    modal?.addEventListener('click', (e) => {
        if (e.target === modal) {
            modal.style.display = 'none';
        }
    });
}

async function createChannel(name, description, custom_link = '') {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        notifications.error('Необходима авторизация');
        return;
    }

    try {
        const requestBody = {
            token,
            name,
            description
        };
        
        if (custom_link) {
            requestBody.custom_link = custom_link;
        }
        
        const response = await fetch('/api/create-channel', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(requestBody)
        });

        const data = await response.json();
        
        console.log('Create channel response:', data);

        if (data.success) {
            notifications.success('Канал создан успешно');
            // Обновляем общий список
            if (typeof loadAllChats === 'function') {
                loadAllChats();
            } else if (typeof loadChats === 'function') {
                loadChats();
            }
        } else {
            const errorMsg = data.error || data.message || 'Ошибка при создании канала';
            console.error('Create channel error:', errorMsg);
            notifications.error(errorMsg);
        }
    } catch (error) {
        console.error('Error creating channel:', error);
        notifications.error('Ошибка при создании канала');
    }
}

async function loadChannels() {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        return;
    }

    try {
        const response = await fetch('/api/get-channels', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ token })
        });

        const data = await response.json();

        if (data.success && data.channels) {
            channels = data.channels;
            renderChannels();
        }
    } catch (error) {
        console.error('Error loading channels:', error);
    }
}

function renderChannels() {
    // Эта функция больше не используется напрямую для рендеринга
    // Каналы рендерятся через loadAllChats() в chat.js
    // Оставляем функцию для обратной совместимости
    return;
}

async function selectChannel(channel) {
    console.log('selectChannel called with:', channel);
    console.log('Channel ID:', channel?.id);
    console.log('Channel name:', channel?.name);
    
    // Если channel это объект из chat.data, используем его напрямую
    if (!channel) {
        console.error('Channel is null or undefined');
        notifications.error('Ошибка: канал не указан');
        return;
    }
    
    if (!channel.id) {
        console.error('Invalid channel object - no ID:', channel);
        notifications.error('Ошибка: неверные данные канала (нет ID)');
        return;
    }

    if (typeof window.stopTypingForCurrentTarget === 'function') {
        window.stopTypingForCurrentTarget();
    }

    currentChannel = channel;
    currentChannelMessages = [];
    channelInfoSelectedMessages.clear();
    updateChannelInfoPanel();
    if (typeof currentChat !== 'undefined') {
        currentChat = null; // Сбрасываем текущий чат
    }
    if (window.groupsModule) {
        window.groupsModule.currentGroup = null;
    }
    if (typeof updatePremiumGiftAvailability === 'function') {
        updatePremiumGiftAvailability();
    }
    if (typeof closePremiumGiftModal === 'function') {
        closePremiumGiftModal();
    }

    if (typeof clearReplyState === 'function') {
        clearReplyState();
    }
    if (typeof clearPendingAttachments === 'function') {
        clearPendingAttachments();
    }
    if (typeof resetReplyKeyboardState === 'function') {
        resetReplyKeyboardState();
    }
    if (typeof persistActiveChatSelection === 'function') {
        persistActiveChatSelection({ id: channel.id, type: 'channel' });
    }

    // Обновляем активный элемент в списке
    document.querySelectorAll('.chat-item').forEach(item => {
        item.classList.remove('active');
    });
    // Ищем элемент по data-chat-id или data-channel-id
    const activeItem = document.querySelector(`[data-chat-id="${channel.id}"][data-chat-type="channel"]`) ||
                      document.querySelector(`[data-channel-id="${channel.id}"]`) ||
                      document.querySelector(`[data-chat-id="${channel.id}"]`);
    if (activeItem) {
        activeItem.classList.add('active');
        console.log('Active item found and marked');
    } else {
        console.warn('Active item not found for channel:', channel.id, 'Available items:', 
            Array.from(document.querySelectorAll('.chat-item')).map(i => ({
                id: i.dataset.chatId || i.dataset.channelId,
                type: i.dataset.chatType || i.dataset.type
            })));
    }

    // Показываем область чата
    const chatHeader = document.getElementById('chatHeader');
    const chatInputArea = document.getElementById('chatInputArea');
    if (chatHeader) chatHeader.style.display = 'flex';
    if (chatInputArea) chatInputArea.style.display = 'block';

    const leaveBtn = document.getElementById('leaveGroupBtn');
    if (leaveBtn) leaveBtn.style.display = 'none';

    // Обновляем заголовок чата
    const chatHeaderName = document.getElementById('chatHeaderName');
    const chatHeaderStatus = document.getElementById('chatHeaderStatus');
    const chatHeaderAvatar = document.getElementById('chatHeaderAvatar');
    
    if (chatHeaderName) {
        chatHeaderName.textContent = channel.name;
        // Добавляем галочку верификации если есть
        updateChannelVerifiedBadge(channel.is_verified);
    }
    
    if (chatHeaderStatus) {
        // Для каналов показываем @username только если он есть и не пустой
        const statusText = channel.custom_link && channel.custom_link.trim() !== ''
            ? '@' + channel.custom_link
            : 'Канал';
        if (typeof window.setChatHeaderStatusBase === 'function') {
            window.setChatHeaderStatusBase(statusText);
        } else {
            chatHeaderStatus.textContent = statusText;
        }
    }
    
    if (chatHeaderAvatar) {
        const avatarUrl = channel.avatar_url || '';
        if (avatarUrl) {
            const img = document.createElement('img');
            img.src = avatarUrl;
            img.style.width = '100%';
            img.style.height = '100%';
            img.style.objectFit = 'cover';
            img.style.borderRadius = '50%';
            img.onerror = () => {
                chatHeaderAvatar.innerHTML = typeof appIcon === 'function' 
                    ? appIcon(channel.is_private ? 'channelPrivate' : 'channel', 'app-icon-lg') 
                    : (channel.is_private ? '🔒' : '📢');
            };
            chatHeaderAvatar.innerHTML = '';
            chatHeaderAvatar.appendChild(img);
        } else {
            chatHeaderAvatar.innerHTML = typeof appIcon === 'function' 
                ? appIcon(channel.is_private ? 'channelPrivate' : 'channel', 'app-icon-lg') 
                : (channel.is_private ? '🔒' : '📢');
        }
    }
    if (typeof window.clearTypingIndicator === 'function') {
        window.clearTypingIndicator();
    }
    
    // Скрываем кнопку звонка для каналов (звонки не имеют смысла для каналов)
    const callBtn = document.getElementById('callBtn');
    if (callBtn) {
        callBtn.style.display = 'none';
    }
    
    // Показываем кнопки поиска и меню только для каналов
    const searchBtn = document.getElementById('searchChannelBtn');
    const menuBtn = document.getElementById('channelMenuBtn');
    if (searchBtn) searchBtn.style.display = 'flex';
    if (menuBtn) {
        // Hide until role is confirmed from backend
        menuBtn.style.display = 'none';
    }
    
    // Удаляем кнопку подписки из обычных чатов (если она там есть)
    const subscribeBtn = document.getElementById('channelSubscribeBtn');
    if (subscribeBtn && !currentChannel) {
        subscribeBtn.remove();
    }

    // Загружаем информацию о канале (роль, подписчики и т.д.)
    await loadChannelInfo(channel.id);
    
    // Показываем/скрываем поле ввода в зависимости от роли
    if (chatInputArea) {
        const perms = currentChannelInfo?.permissions || 0;
        const role = currentChannelInfo?.user_role;
        const canPost = (perms & (1 << 1)) !== 0 || role === 'owner' || role === 'creator' || currentChannelInfo?.is_admin;
        chatInputArea.style.display = canPost ? 'block' : 'none';
    }
    
    // Загружаем сообщения канала
    console.log('Loading messages for channel:', channel.id);
    await loadChannelMessages(channel.id);
    console.log('Channel selected successfully:', channel.name);
}

// Загрузка информации о канале
async function loadChannelInfo(channelId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    // Hide topic tabs when switching to channel
    if (window.topicsModule && typeof window.topicsModule.hideTopicTabs === 'function') {
        window.topicsModule.hideTopicTabs();
    }
    window.currentActiveTopic = null;
    
    try {
        const response = await fetch('/api/get-channel-info', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId })
        });
        
        const data = await response.json();
        if (data.success) {
            currentChannelInfo = data;
            console.log('Channel info loaded:', currentChannelInfo);

            if (data.channel && currentChannel) {
                currentChannel.name = data.channel.name || currentChannel.name;
                currentChannel.description = data.channel.description || currentChannel.description;
                currentChannel.custom_link = data.channel.custom_link || currentChannel.custom_link;
                if (data.channel.avatar_url !== undefined) {
                    currentChannel.avatar_url = data.channel.avatar_url;
                }
                if (typeof data.channel.is_private === 'boolean') {
                    currentChannel.is_private = data.channel.is_private;
                }
                if (typeof data.channel.show_author === 'boolean') {
                    currentChannel.show_author = data.channel.show_author;
                }
            }

            const chatHeaderAvatar = document.getElementById('chatHeaderAvatar');
            if (chatHeaderAvatar) {
                const avatarUrl = currentChannel?.avatar_url || '';
                if (avatarUrl) {
                    const img = document.createElement('img');
                    img.src = avatarUrl;
                    img.style.width = '100%';
                    img.style.height = '100%';
                    img.style.objectFit = 'cover';
                    img.style.borderRadius = '50%';
                    img.onerror = () => {
                        chatHeaderAvatar.innerHTML = typeof appIcon === 'function' 
                            ? appIcon(currentChannel?.is_private ? 'channelPrivate' : 'channel', 'app-icon-lg') 
                            : (currentChannel?.is_private ? '🔒' : '📢');
                    };
                    chatHeaderAvatar.innerHTML = '';
                    chatHeaderAvatar.appendChild(img);
                }
            }
            
            // Обновляем кнопку подписки/отписки в заголовке
            updateSubscribeButton();
            updateChannelMenuVisibility();
            updateChannelInfoPanel();
            // Обновляем галочку верификации после загрузки данных с сервера
            updateChannelVerifiedBadge(currentChannelInfo?.channel?.is_verified);
        }
    } catch (error) {
        console.error('Error loading channel info:', error);
    }
}

function hasChannelSettingsAccess() {
    const role = currentChannelInfo?.user_role;
    if (!currentChannelInfo || currentChannelInfo?.is_admin) return !!currentChannelInfo?.is_admin;
    return role === 'admin' || role === 'owner' || role === 'creator';
}

function updateChannelMenuVisibility() {
    const menuBtn = document.getElementById('channelMenuBtn');
    if (!menuBtn) return;
    if (hasChannelSettingsAccess()) {
        menuBtn.style.display = 'flex';
    } else {
        menuBtn.style.display = 'none';
    }
}

// Обновление бейджа верификации в заголовке канала
function updateChannelVerifiedBadge(isVerified) {
    // Удаляем старый бейдж если есть
    const oldBadge = document.getElementById('channelVerifiedBadge');
    if (oldBadge) oldBadge.remove();
    
    if (!isVerified) return;
    
    const chatHeaderName = document.getElementById('chatHeaderName');
    if (!chatHeaderName) return;
    
    // Создаем бейдж верификации как в Telegram
    const badge = document.createElement('span');
    badge.id = 'channelVerifiedBadge';
    badge.className = 'channel-verified-badge';
    badge.innerHTML = `<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="10" fill="currentColor"/><path d="M8.5 12.5L10.5 14.5L15.5 9.5" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>`;
    badge.title = 'Верифицированный канал';
    
    // Вставляем после текста имени (appendChild добавит в конец h3)
    chatHeaderName.appendChild(badge);
}

// Обновление кнопки подписки/отписки
function updateSubscribeButton() {
    // Удаляем старую кнопку если есть
    const oldBtn = document.getElementById('channelSubscribeBtn');
    if (oldBtn) oldBtn.remove();
    
    // Проверяем, что мы в канале, а не в обычном чате
    if (!currentChannelInfo || !currentChannel) {
        return;
    }
    
    // Дополнительная проверка: убеждаемся, что currentChannel это действительно канал
    // Проверяем через channelsModule
    if (window.channelsModule && !window.channelsModule.isChannelActive()) {
        return;
    }
    
    const chatHeader = document.getElementById('chatHeader');
    if (!chatHeader) return;
    
    // Создаем кнопку подписки/отписки
    const subscribeBtn = document.createElement('button');
    subscribeBtn.id = 'channelSubscribeBtn';
    subscribeBtn.className = 'btn-primary';
    subscribeBtn.style.cssText = 'margin-left: auto; padding: 0.5rem 1rem; font-size: 0.9rem;';
    
    const role = currentChannelInfo?.user_role || '';
    const isOwnerOrAdmin = role === 'owner' || role === 'creator' || role === 'admin' || currentChannelInfo?.is_admin;
    if (isOwnerOrAdmin) {
        return;
    }

    if (currentChannelInfo.is_subscribed) {
        subscribeBtn.textContent = 'Отписаться';
        subscribeBtn.onclick = () => unsubscribeFromChannel(currentChannel.id);
    } else {
        subscribeBtn.textContent = 'Подписаться';
        subscribeBtn.onclick = () => subscribeToChannel(currentChannel.id);
    }
    
    // Добавляем кнопку в заголовок
    chatHeader.appendChild(subscribeBtn);
}

// Отписка от канала
async function unsubscribeFromChannel(channelId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        const response = await fetch('/api/unsubscribe-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId })
        });
        
        const data = await response.json();
        if (data.success) {
            notifications.success('Вы отписались от канала');
            // Перезагружаем информацию о канале
            await loadChannelInfo(channelId);
            // Перезагружаем список каналов
            await loadChannels();
        } else {
            notifications.error(data.error || 'Ошибка отписки от канала');
        }
    } catch (error) {
        console.error('Unsubscribe error:', error);
        notifications.error('Ошибка отписки от канала');
    }
}

function formatSubscriberCount(count) {
    const num = Number(count) || 0;
    const abs = Math.abs(num) % 100;
    const last = abs % 10;
    if (abs > 10 && abs < 20) return `${num} подписчиков`;
    if (last === 1) return `${num} подписчик`;
    if (last > 1 && last < 5) return `${num} подписчика`;
    return `${num} подписчиков`;
}

function getChannelSelfId() {
    return localStorage.getItem('xipher_user_id') || '';
}

function getChannelRole() {
    return currentChannelInfo?.user_role || '';
}

function isChannelOwnerRole(role) {
    return role === 'owner' || role === 'creator';
}

function getChannelPermissionValue() {
    return Number(currentChannelInfo?.permissions || 0);
}

function channelHasPermission(bit) {
    const role = getChannelRole();
    if (isChannelOwnerRole(role)) return true;
    if (channelSupportsAdminPerms) {
        const perms = getChannelPermissionValue();
        return (perms & bit) !== 0;
    }
    if (bit === CHANNEL_ADMIN_PERMS.PROMOTE) {
        return false;
    }
    return currentChannelInfo?.is_admin || role === 'admin';
}

function formatChannelRoleLabel(role) {
    switch (role) {
        case 'owner':
        case 'creator':
            return 'Владелец';
        case 'admin':
            return 'Администратор';
        default:
            return 'Подписчик';
    }
}

function formatAdminPermsShort(perms) {
    const value = Number(perms || 0);
    if (!value) return '';
    const labels = CHANNEL_ADMIN_PERM_OPTIONS
        .filter(item => (value & item.bit) !== 0)
        .map(item => item.short || item.label);
    return labels.join(', ');
}

function buildChannelInviteUrl(token) {
    if (!token) return '';
    return `${window.location.origin}/join/${token}`;
}

function copyToClipboard(text) {
    if (!text) return;
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text)
            .then(() => notifications.success('Скопировано'))
            .catch(() => notifications.error('Не удалось скопировать'));
        return;
    }
    const area = document.createElement('textarea');
    area.value = text;
    area.style.position = 'fixed';
    area.style.left = '-9999px';
    document.body.appendChild(area);
    area.focus();
    area.select();
    try {
        document.execCommand('copy');
        notifications.success('Скопировано');
    } catch (err) {
        notifications.error('Не удалось скопировать');
    } finally {
        document.body.removeChild(area);
    }
}

function extractUrlsFromText(text) {
    if (!text) return [];
    const regex = /(?:https?:\/\/|www\.)[^\s<]+/gi;
    const matches = text.match(regex) || [];
    return matches.map(raw => raw.replace(/[),.!?]+$/g, ''));
}

function classifyChannelMedia(messages) {
    const photos = [];
    const videos = [];
    const voices = [];
    const links = [];
    const imageExts = ['jpg', 'jpeg', 'png', 'gif', 'webp', 'bmp', 'svg'];
    const videoExts = ['mp4', 'webm', 'ogg', 'mov', 'avi', 'mkv'];

    (messages || []).forEach(msg => {
        if (msg.message_type === 'file' && msg.file_path) {
            const name = (msg.file_name || msg.file_path || '').toLowerCase();
            const ext = name.includes('.') ? name.split('.').pop() : '';
            if (imageExts.includes(ext)) {
                photos.push(msg);
            } else if (videoExts.includes(ext)) {
                videos.push(msg);
            }
        } else if (msg.message_type === 'voice' && msg.file_path) {
            voices.push(msg);
        }

        const urls = extractUrlsFromText(msg.content || '');
        if (urls.length) {
            urls.forEach(url => links.push({ url, message: msg }));
        }
    });

    return { photos, videos, voices, links };
}

let channelInfoMediaCache = { photos: [], videos: [], voices: [], links: [] };
let channelInfoContextMenu = null;
let channelInfoContextState = { message: null, element: null };
let channelMemberContextMenu = null;
let channelMemberContextState = { member: null };

function ensureChannelInfoContextMenu() {
    if (channelInfoContextMenu) return;
    channelInfoContextMenu = document.createElement('div');
    channelInfoContextMenu.className = 'message-context-menu';
    channelInfoContextMenu.id = 'channelInfoContextMenu';
    document.body.appendChild(channelInfoContextMenu);
    channelInfoContextMenu.addEventListener('contextmenu', (e) => e.preventDefault());
}

function closeChannelInfoContextMenu() {
    if (!channelInfoContextMenu) return;
    channelInfoContextMenu.style.display = 'none';
    document.removeEventListener('click', closeChannelInfoContextMenu, { capture: true });
    channelInfoContextState = { message: null, element: null };
}

function openChannelInfoContextMenu(event, message, element) {
    event.preventDefault();
    ensureChannelInfoContextMenu();
    channelInfoContextState = { message, element };

    const items = [
        {
            label: 'Перейти к сообщению',
            icon: '↩️',
            action: () => scrollToChannelMessage(message?.id)
        },
        {
            label: 'Переслать',
            icon: '➡️',
            action: () => {
                if (typeof showForwardMessageModal === 'function') {
                    showForwardMessageModal(message);
                } else {
                    notifications.info('Пересылка недоступна');
                }
            }
        },
        {
            label: 'Выбрать',
            icon: '✅',
            action: () => toggleChannelInfoSelection(message?.id, element)
        }
    ];

    channelInfoContextMenu.innerHTML = '';
    items.forEach(item => {
        const row = document.createElement('div');
        row.className = 'context-menu-item';
        const iconHtml = typeof emojiToIcon === 'function' ? emojiToIcon(item.icon) : item.icon;
        row.innerHTML = `<span>${iconHtml}</span><span>${item.label}</span>`;
        row.addEventListener('click', () => {
            item.action();
            closeChannelInfoContextMenu();
        });
        channelInfoContextMenu.appendChild(row);
    });

    channelInfoContextMenu.style.visibility = 'hidden';
    channelInfoContextMenu.style.display = 'block';
    const rect = channelInfoContextMenu.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    let x = event.clientX;
    let y = event.clientY;
    if (x + rect.width > viewportWidth) {
        x = Math.max(8, viewportWidth - rect.width - 8);
    }
    if (y + rect.height > viewportHeight) {
        y = Math.max(8, viewportHeight - rect.height - 8);
    }
    channelInfoContextMenu.style.left = `${x}px`;
    channelInfoContextMenu.style.top = `${y}px`;
    channelInfoContextMenu.style.visibility = 'visible';
    document.addEventListener('click', closeChannelInfoContextMenu, { capture: true });
}

function ensureChannelMemberContextMenu() {
    if (channelMemberContextMenu) return;
    channelMemberContextMenu = document.createElement('div');
    channelMemberContextMenu.className = 'message-context-menu';
    channelMemberContextMenu.id = 'channelMemberContextMenu';
    document.body.appendChild(channelMemberContextMenu);
    channelMemberContextMenu.addEventListener('contextmenu', (e) => e.preventDefault());
}

function closeChannelMemberContextMenu() {
    if (!channelMemberContextMenu) return;
    channelMemberContextMenu.style.display = 'none';
    document.removeEventListener('click', closeChannelMemberContextMenu, { capture: true });
    channelMemberContextState = { member: null };
}

function openChannelMemberContextMenu(event, member) {
    event.preventDefault();
    ensureChannelMemberContextMenu();
    channelMemberContextState = { member };

    const items = [];
    items.push({
        label: 'Посмотреть профиль',
        icon: '👤',
        action: () => {
            if (typeof window.openUserProfile === 'function') {
                window.openUserProfile({ user_id: member.user_id, username: member.username });
            } else {
                notifications.info('Профиль недоступен');
            }
        }
    });

    const selfId = getChannelSelfId();
    const isSelf = selfId && member.user_id === selfId;
    const role = member.role || '';
    const isOwner = isChannelOwnerRole(role);
    const isAdmin = role === 'admin';
    const isBanned = !!member.is_banned;

    if (!isSelf && !isOwner) {
        const canPromote = channelHasPermission(CHANNEL_ADMIN_PERMS.PROMOTE);
        const canRestrict = channelHasPermission(CHANNEL_ADMIN_PERMS.RESTRICT);

        if (canPromote) {
            if (isAdmin) {
                items.push({
                    label: 'Изменить права',
                    icon: '🛠️',
                    action: () => openChannelAdminPermModal(member)
                });
                items.push({
                    label: 'Снять права админа',
                    icon: '↘️',
                    action: () => demoteChannelAdmin(member)
                });
            } else if (!isBanned) {
                items.push({
                    label: 'Сделать админом',
                    icon: '⭐',
                    action: () => openChannelAdminPermModal(member, { promote: true })
                });
            }
        }

        if (canRestrict) {
            if (isBanned) {
                items.push({
                    label: 'Разблокировать',
                    icon: '♻️',
                    action: () => setChannelMemberBan(member, false)
                });
            } else {
                items.push({
                    label: 'Заблокировать',
                    icon: '⛔',
                    action: () => setChannelMemberBan(member, true)
                });
            }
        }
    }

    channelMemberContextMenu.innerHTML = '';
    items.forEach(item => {
        const row = document.createElement('div');
        row.className = 'context-menu-item';
        const iconHtml = typeof emojiToIcon === 'function' ? emojiToIcon(item.icon) : item.icon;
        row.innerHTML = `<span>${iconHtml}</span><span>${item.label}</span>`;
        row.addEventListener('click', () => {
            item.action();
            closeChannelMemberContextMenu();
        });
        channelMemberContextMenu.appendChild(row);
    });

    channelMemberContextMenu.style.visibility = 'hidden';
    channelMemberContextMenu.style.display = 'block';
    const rect = channelMemberContextMenu.getBoundingClientRect();
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    let x = event.clientX;
    let y = event.clientY;
    if (x + rect.width > viewportWidth) {
        x = Math.max(8, viewportWidth - rect.width - 8);
    }
    if (y + rect.height > viewportHeight) {
        y = Math.max(8, viewportHeight - rect.height - 8);
    }
    channelMemberContextMenu.style.left = `${x}px`;
    channelMemberContextMenu.style.top = `${y}px`;
    channelMemberContextMenu.style.visibility = 'visible';
    document.addEventListener('click', closeChannelMemberContextMenu, { capture: true });
}

function ensureChannelAdminPermGrid() {
    const grid = document.getElementById('channelAdminPermGrid');
    if (!grid || grid.dataset.ready === 'true') return;
    grid.innerHTML = '';
    CHANNEL_ADMIN_PERM_OPTIONS.forEach(option => {
        const label = document.createElement('label');
        label.className = 'channel-perm-item';
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.dataset.permBit = option.bit;
        const text = document.createElement('div');
        text.innerHTML = `<div style="font-weight:600;">${escapeHtml(option.label)}</div>` +
            `<div style="color:var(--text-secondary);font-size:12px;">${escapeHtml(option.hint)}</div>`;
        label.appendChild(checkbox);
        label.appendChild(text);
        grid.appendChild(label);
    });
    grid.dataset.ready = 'true';
}

function setChannelAdminPermSelections(perms) {
    const grid = document.getElementById('channelAdminPermGrid');
    if (!grid) return;
    grid.querySelectorAll('input[data-perm-bit]').forEach(input => {
        const bit = Number(input.dataset.permBit || 0);
        input.checked = bit !== 0 && (perms & bit) !== 0;
    });
}

function getChannelAdminPermSelections() {
    const grid = document.getElementById('channelAdminPermGrid');
    if (!grid) return 0;
    let perms = 0;
    grid.querySelectorAll('input[data-perm-bit]').forEach(input => {
        const bit = Number(input.dataset.permBit || 0);
        if (input.checked && bit) perms |= bit;
    });
    return perms;
}

function openChannelAdminPermModal(member, options = {}) {
    if (!member || !currentChannel) return;
    const modal = document.getElementById('channelAdminPermModal');
    if (!modal) return;
    ensureChannelAdminPermGrid();
    channelAdminPermModalState = { member };
    const subtitle = document.getElementById('channelAdminPermSubtitle');
    if (subtitle) {
        const name = member.username ? `@${member.username}` : member.user_id;
        subtitle.textContent = `Права для ${name}`;
    }
    const titleInput = document.getElementById('channelAdminPermTitle');
    if (titleInput) titleInput.value = member.admin_title || '';
    const isAdmin = member.role === 'admin';
    const fallbackPerms = isAdmin ? Number(member.admin_perms || 0) : DEFAULT_CHANNEL_ADMIN_PERMS;
    const perms = typeof options.perms === 'number' ? options.perms : fallbackPerms;
    setChannelAdminPermSelections(perms || DEFAULT_CHANNEL_ADMIN_PERMS);
    modal.style.display = 'flex';
}

function closeChannelAdminPermModal() {
    const modal = document.getElementById('channelAdminPermModal');
    if (modal) modal.style.display = 'none';
    channelAdminPermModalState = { member: null };
}

async function saveChannelAdminPerms() {
    const member = channelAdminPermModalState.member;
    if (!member || !currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    const perms = getChannelAdminPermSelections();
    if (perms === 0) {
        const confirmed = confirm('Снять права администратора?');
        if (!confirmed) return;
    }
    const body = {
        token,
        channel_id: currentChannel.id,
        target_user_id: member.user_id,
        perms
    };
    const title = document.getElementById('channelAdminPermTitle')?.value.trim();
    if (title) {
        body.title = title;
    }
    if (perms === 0) {
        body.revoke = true;
    }
    try {
        const resp = await fetch('/api/set-channel-admin-permissions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        const data = await resp.json();
        if (data.success) {
            notifications.success(perms === 0 ? 'Администратор снят' : 'Права администратора обновлены');
            closeChannelAdminPermModal();
            loadChannelMembers2025();
        } else {
            notifications.error(data.error || data.message || 'Не удалось обновить права');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сети при обновлении прав');
    }
}

async function demoteChannelAdmin(member) {
    if (!member || !currentChannel) return;
    const confirmed = confirm(`Снять права администратора у @${member.username || member.user_id}?`);
    if (!confirmed) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const resp = await fetch('/api/set-channel-admin-permissions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token,
                channel_id: currentChannel.id,
                target_user_id: member.user_id,
                perms: 0,
                revoke: true
            })
        });
        const data = await resp.json();
        if (data.success) {
            notifications.success('Права администратора сняты');
            loadChannelMembers2025();
        } else {
            notifications.error(data.error || data.message || 'Не удалось снять права');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сети при снятии прав');
    }
}

async function setChannelMemberBan(member, banned) {
    if (!member || !currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    const actionLabel = banned ? 'заблокировать' : 'разблокировать';
    const confirmed = confirm(`${actionLabel.charAt(0).toUpperCase() + actionLabel.slice(1)} пользователя @${member.username || member.user_id}?`);
    if (!confirmed) return;
    try {
        const resp = await fetch('/api/ban-channel-member', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token,
                channel_id: currentChannel.id,
                target_user_id: member.user_id,
                banned: banned ? 'true' : 'false'
            })
        });
        const data = await resp.json();
        if (data.success) {
            notifications.success(banned ? 'Пользователь заблокирован' : 'Пользователь разблокирован');
            loadChannelMembers2025();
        } else {
            notifications.error(data.error || data.message || 'Не удалось обновить статус');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сети при изменении статуса');
    }
}

function toggleChannelInfoSelection(messageId, element) {
    if (!messageId || !element) return;
    if (channelInfoSelectedMessages.has(messageId)) {
        channelInfoSelectedMessages.delete(messageId);
        element.classList.remove('selected');
    } else {
        channelInfoSelectedMessages.add(messageId);
        element.classList.add('selected');
    }
}

function scrollToChannelMessage(messageId) {
    if (!messageId) return;
    const el = document.querySelector(`.message[data-message-id="${messageId}"]`);
    if (!el) {
        notifications.info('Сообщение не найдено в загруженной истории');
        return;
    }
    el.scrollIntoView({ behavior: 'smooth', block: 'center' });
    el.classList.add('message-highlight');
    setTimeout(() => el.classList.remove('message-highlight'), 1600);
}

function openChannelMediaViewer(type, src, name = '') {
    if (!src) return;
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed; inset:0; background:rgba(0,0,0,0.92); z-index:10000; display:flex; align-items:center; justify-content:center; padding:2rem; cursor:pointer;';
    let content;
    if (type === 'video') {
        content = document.createElement('video');
        content.src = src;
        content.controls = true;
        content.autoplay = true;
        content.style.maxWidth = '90%';
        content.style.maxHeight = '90%';
        content.style.borderRadius = '12px';
    } else {
        content = document.createElement('img');
        content.src = src;
        content.alt = name || 'media';
        content.style.maxWidth = '90%';
        content.style.maxHeight = '90%';
        content.style.objectFit = 'contain';
        content.style.borderRadius = '12px';
    }
    content.addEventListener('click', (e) => e.stopPropagation());
    overlay.appendChild(content);
    overlay.addEventListener('click', () => overlay.remove());
    document.body.appendChild(overlay);
}

function renderChannelMediaSection({ title, icon, count, renderBody }) {
    const details = document.createElement('details');
    details.className = 'channel-media-section';
    const summary = document.createElement('summary');
    const label = document.createElement('span');
    // Используем SVG иконки
    const iconHtml = typeof emojiToIcon === 'function' ? emojiToIcon(icon) : icon;
    label.innerHTML = `${iconHtml} ${title}`;
    const badge = document.createElement('span');
    badge.className = 'channel-info-subtitle';
    badge.textContent = count.toString();
    summary.appendChild(label);
    summary.appendChild(badge);
    details.appendChild(summary);
    renderBody(details);
    return details;
}

function renderChannelInfoContent(container) {
    if (!container) return;
    container.innerHTML = '';

    if (!currentChannel) {
        return;
    }

    const info = currentChannelInfo?.channel || {};
    const channelName = info.name || currentChannel.name || 'Канал';
    const channelDescription = info.description || currentChannel.description || '';
    const customLink = info.custom_link || currentChannel.custom_link || '';
    const isPrivate = typeof info.is_private === 'boolean' ? info.is_private : !!currentChannel.is_private;
    const role = currentChannelInfo?.user_role || '';
    const countRaw = Number(currentChannelInfo?.subscribers_count || 0);
    const totalRaw = Number(currentChannelInfo?.total_members || 0);
    const displayCount = countRaw > 0 ? countRaw : totalRaw;

    const header = document.createElement('div');
    header.className = 'channel-info-header';

    const avatar = document.createElement('div');
    avatar.className = 'channel-info-avatar';
    const avatarUrl = info.avatar_url || currentChannel.avatar_url || '';
    if (avatarUrl) {
        const img = document.createElement('img');
        img.src = avatarUrl;
        img.onerror = () => {
            avatar.textContent = (channelName || 'К').charAt(0).toUpperCase();
        };
        avatar.appendChild(img);
    } else {
        avatar.textContent = (channelName || 'К').charAt(0).toUpperCase();
    }

    const meta = document.createElement('div');
    meta.className = 'channel-info-meta';

    const title = document.createElement('div');
    title.className = 'channel-info-title';
    title.textContent = channelName;

    const subtitle = document.createElement('div');
    subtitle.className = 'channel-info-subtitle';
    subtitle.textContent = formatSubscriberCount(displayCount);

    meta.appendChild(title);
    meta.appendChild(subtitle);
    header.appendChild(avatar);
    header.appendChild(meta);
    container.appendChild(header);

    const linkSection = document.createElement('div');
    linkSection.className = 'channel-info-section';
    const linkLabel = document.createElement('div');
    linkLabel.className = 'channel-info-label';
    linkLabel.textContent = 'Ссылка';
    const linkRow = document.createElement('div');
    linkRow.className = 'channel-info-link';
    if (customLink) {
        const link = document.createElement('a');
        link.href = `/@${customLink}`;
        link.textContent = `@${customLink}`;
        linkRow.appendChild(link);

        const copyBtn = document.createElement('button');
        copyBtn.className = 'btn-secondary btn-small';
        copyBtn.textContent = 'Копировать';
        copyBtn.addEventListener('click', (e) => {
            e.preventDefault();
            copyToClipboard(`${window.location.origin}/@${customLink}`);
        });
        linkRow.appendChild(copyBtn);
    } else {
        const noLink = document.createElement('div');
        noLink.className = 'channel-info-subtitle';
        noLink.textContent = isPrivate ? 'Приватный канал' : 'Ссылка не задана';
        linkRow.appendChild(noLink);
    }
    linkSection.appendChild(linkLabel);
    linkSection.appendChild(linkRow);
    container.appendChild(linkSection);

    const descSection = document.createElement('div');
    descSection.className = 'channel-info-section';
    const descLabel = document.createElement('div');
    descLabel.className = 'channel-info-label';
    descLabel.textContent = 'Описание';
    const descText = document.createElement('div');
    descText.className = 'channel-info-subtitle';
    descText.textContent = channelDescription || 'Описание отсутствует';
    descSection.appendChild(descLabel);
    descSection.appendChild(descText);
    container.appendChild(descSection);

    const notifSection = document.createElement('div');
    notifSection.className = 'channel-info-section';
    const notifRow = document.createElement('div');
    notifRow.className = 'channel-info-toggle';
    const notifLabel = document.createElement('div');
    notifLabel.textContent = 'Уведомления';
    const switchLabel = document.createElement('label');
    switchLabel.className = 'switch';
    const input = document.createElement('input');
    input.type = 'checkbox';
    const slider = document.createElement('span');
    slider.className = 'switch-slider';
    const isMuted = typeof isChatMuted === 'function' ? isChatMuted({ id: currentChannel.id, type: 'channel' }) : false;
    input.checked = !isMuted;
    input.addEventListener('change', () => {
        if (typeof setChatMuted === 'function') {
            setChatMuted({ id: currentChannel.id, type: 'channel' }, !input.checked);
        }
    });
    switchLabel.appendChild(input);
    switchLabel.appendChild(slider);
    notifRow.appendChild(notifLabel);
    notifRow.appendChild(switchLabel);
    notifSection.appendChild(notifRow);
    container.appendChild(notifSection);

    const spacer = document.createElement('div');
    spacer.className = 'channel-info-divider';
    container.appendChild(spacer);

    const media = channelInfoMediaCache;

    const photoSection = renderChannelMediaSection({
        title: 'Фото',
        icon: '🖼️',
        count: media.photos.length,
        renderBody: (details) => {
            const body = document.createElement('div');
            body.className = 'channel-media-grid';
            if (!media.photos.length) {
                const empty = document.createElement('div');
                empty.className = 'channel-info-subtitle';
                empty.textContent = 'Пока нет фото';
                body.appendChild(empty);
            } else {
                media.photos.forEach(msg => {
                    const item = document.createElement('div');
                    item.className = 'channel-media-item';
                    const thumb = document.createElement('div');
                    thumb.className = 'channel-media-thumb';
                    const fileUrl = typeof getSafeFileUrl === 'function' ? getSafeFileUrl(msg.file_path) : `/files/${msg.file_path}`;
                    if (fileUrl) {
                        const img = document.createElement('img');
                        img.src = fileUrl;
                        img.alt = msg.file_name || 'Фото';
                        thumb.appendChild(img);
                    } else {
                        thumb.textContent = 'Фото';
                    }
                    const name = document.createElement('div');
                    name.className = 'channel-media-name';
                    name.textContent = msg.file_name || 'Фото';
                    item.appendChild(thumb);
                    item.appendChild(name);
                    item.addEventListener('click', () => openChannelMediaViewer('image', fileUrl, msg.file_name));
                    item.addEventListener('contextmenu', (e) => openChannelInfoContextMenu(e, msg, item));
                    body.appendChild(item);
                });
            }
            details.appendChild(body);
        }
    });
    container.appendChild(photoSection);

    const videoSection = renderChannelMediaSection({
        title: 'Видео',
        icon: '🎬',
        count: media.videos.length,
        renderBody: (details) => {
            const body = document.createElement('div');
            body.className = 'channel-media-grid';
            if (!media.videos.length) {
                const empty = document.createElement('div');
                empty.className = 'channel-info-subtitle';
                empty.textContent = 'Пока нет видео';
                body.appendChild(empty);
            } else {
                media.videos.forEach(msg => {
                    const item = document.createElement('div');
                    item.className = 'channel-media-item';
                    const thumb = document.createElement('div');
                    thumb.className = 'channel-media-thumb';
                    thumb.textContent = '▶️';
                    const name = document.createElement('div');
                    name.className = 'channel-media-name';
                    name.textContent = msg.file_name || 'Видео';
                    const fileUrl = typeof getSafeFileUrl === 'function' ? getSafeFileUrl(msg.file_path) : `/files/${msg.file_path}`;
                    item.appendChild(thumb);
                    item.appendChild(name);
                    item.addEventListener('click', () => openChannelMediaViewer('video', fileUrl, msg.file_name));
                    item.addEventListener('contextmenu', (e) => openChannelInfoContextMenu(e, msg, item));
                    body.appendChild(item);
                });
            }
            details.appendChild(body);
        }
    });
    container.appendChild(videoSection);

    const linkSectionMedia = renderChannelMediaSection({
        title: 'Ссылки',
        icon: '🔗',
        count: media.links.length,
        renderBody: (details) => {
            const body = document.createElement('div');
            body.className = 'channel-link-list';
            if (!media.links.length) {
                const empty = document.createElement('div');
                empty.className = 'channel-info-subtitle';
                empty.textContent = 'Пока нет ссылок';
                body.appendChild(empty);
            } else {
                media.links.forEach(entry => {
                    const item = document.createElement('div');
                    item.className = 'channel-link-item';
                    const link = document.createElement('a');
                    const href = entry.url.startsWith('http') ? entry.url : `https://${entry.url}`;
                    link.href = href;
                    link.target = '_blank';
                    link.rel = 'noopener noreferrer';
                    link.textContent = entry.url;
                    const snippet = document.createElement('div');
                    snippet.className = 'channel-info-subtitle';
                    const content = entry.message?.content || '';
                    snippet.textContent = content.length > 120 ? `${content.slice(0, 120)}…` : content;
                    item.appendChild(link);
                    if (snippet.textContent) item.appendChild(snippet);
                    item.addEventListener('contextmenu', (e) => openChannelInfoContextMenu(e, entry.message, item));
                    body.appendChild(item);
                });
            }
            details.appendChild(body);
        }
    });
    container.appendChild(linkSectionMedia);

    const voiceSection = renderChannelMediaSection({
        title: 'Голосовые',
        icon: '🎤',
        count: media.voices.length,
        renderBody: (details) => {
            const body = document.createElement('div');
            body.className = 'channel-link-list';
            if (!media.voices.length) {
                const empty = document.createElement('div');
                empty.className = 'channel-info-subtitle';
                empty.textContent = 'Пока нет голосовых';
                body.appendChild(empty);
            } else {
                media.voices.forEach(msg => {
                    const item = document.createElement('div');
                    item.className = 'channel-voice-item';
                    const fileUrl = typeof getSafeFileUrl === 'function' ? getSafeFileUrl(msg.file_path) : `/files/${msg.file_path}`;
                    if (fileUrl) {
                        const audio = document.createElement('audio');
                        audio.controls = true;
                        audio.src = fileUrl;
                        item.appendChild(audio);
                    } else {
                        const fallback = document.createElement('div');
                        fallback.className = 'channel-info-subtitle';
                        fallback.textContent = 'Голосовое недоступно';
                        item.appendChild(fallback);
                    }
                    const meta = document.createElement('div');
                    meta.className = 'channel-info-subtitle';
                    meta.textContent = msg.file_name || 'Голосовое сообщение';
                    item.appendChild(meta);
                    item.addEventListener('contextmenu', (e) => openChannelInfoContextMenu(e, msg, item));
                    body.appendChild(item);
                });
            }
            details.appendChild(body);
        }
    });
    container.appendChild(voiceSection);

    const spacerTwo = document.createElement('div');
    spacerTwo.className = 'channel-info-divider';
    container.appendChild(spacerTwo);

    const actions = document.createElement('div');
    actions.className = 'channel-info-actions';
    const canLeave = !(role === 'owner' || role === 'creator' || role === 'admin' || currentChannelInfo?.is_admin);
    if (canLeave) {
        const leaveBtn = document.createElement('button');
        leaveBtn.className = 'btn-danger';
        leaveBtn.textContent = 'Выйти с канала';
        leaveBtn.addEventListener('click', () => {
            const confirmed = confirm('Выйти из канала?');
            if (confirmed) {
                unsubscribeFromChannel(currentChannel.id);
            }
        });
        actions.appendChild(leaveBtn);
    }

    const reportBtn = document.createElement('button');
    reportBtn.className = 'btn-secondary';
    reportBtn.textContent = 'Пожаловаться';
    reportBtn.addEventListener('click', () => {
        const latest = currentChannelMessages[currentChannelMessages.length - 1];
        if (latest && typeof openReportModal === 'function') {
            openReportModal(latest);
        } else {
            notifications.info('Жалоба на канал пока не поддерживается');
        }
    });
    actions.appendChild(reportBtn);
    container.appendChild(actions);

    if (currentChannelInfo?.is_admin || role === 'owner' || role === 'creator' || role === 'admin') {
        const adminSection = document.createElement('div');
        adminSection.className = 'channel-info-section';
        const adminLabel = document.createElement('div');
        adminLabel.className = 'channel-info-label';
        adminLabel.textContent = 'Администрирование';
        const adminActions = document.createElement('div');
        adminActions.className = 'channel-admin-actions';

        const openTab = (tab) => {
            if (typeof openChannelAdminPanel === 'function') {
                openChannelAdminPanel();
                if (typeof switchChannelTab === 'function') {
                    switchChannelTab(tab);
                }
            }
        };

        const btnSubscribers = document.createElement('button');
        btnSubscribers.className = 'btn-secondary btn-small';
        btnSubscribers.textContent = 'Подписчики';
        btnSubscribers.addEventListener('click', () => openTab('subscribers'));

        const btnAdmins = document.createElement('button');
        btnAdmins.className = 'btn-secondary btn-small';
        btnAdmins.textContent = 'Администраторы';
        btnAdmins.addEventListener('click', () => openTab('admins'));

        const btnSettings = document.createElement('button');
        btnSettings.className = 'btn-secondary btn-small';
        btnSettings.textContent = 'Настройки';
        btnSettings.addEventListener('click', () => openTab('general'));

        adminActions.appendChild(btnSubscribers);
        adminActions.appendChild(btnAdmins);
        adminActions.appendChild(btnSettings);
        adminSection.appendChild(adminLabel);
        adminSection.appendChild(adminActions);
        container.appendChild(adminSection);
    }
}

function updateChannelInfoPanel() {
    const defaultPanel = document.getElementById('infoPanelDefault');
    const channelPanel = document.getElementById('infoPanelChannel');
    if (!defaultPanel || !channelPanel) return;

    if (!currentChannel) {
        defaultPanel.style.display = '';
        channelPanel.style.display = 'none';
        return;
    }

    defaultPanel.style.display = 'none';
    channelPanel.style.display = '';
    channelInfoMediaCache = classifyChannelMedia(currentChannelMessages);
    renderChannelInfoContent(channelPanel);

    const overlayContent = document.getElementById('channelInfoOverlayContent');
    if (overlayContent) {
        renderChannelInfoContent(overlayContent);
    }
}

function openChannelInfoPanel() {
    updateChannelInfoPanel();
    const overlay = document.getElementById('channelInfoOverlay');
    if (!overlay) return;
    const isMobile = window.matchMedia && window.matchMedia('(max-width: 980px)').matches;
    if (!isMobile) return;
    overlay.classList.add('active');
    overlay.setAttribute('aria-hidden', 'false');
}

function closeChannelInfoOverlay() {
    const overlay = document.getElementById('channelInfoOverlay');
    if (!overlay) return;
    overlay.classList.remove('active');
    overlay.setAttribute('aria-hidden', 'true');
}

function setupChannelInfoOverlay() {
    const overlay = document.getElementById('channelInfoOverlay');
    const closeBtn = document.getElementById('closeChannelInfoOverlay');
    if (!overlay) return;
    closeBtn?.addEventListener('click', closeChannelInfoOverlay);
    overlay.addEventListener('click', (e) => {
        if (e.target === overlay) closeChannelInfoOverlay();
    });
}

// NOTE: duplicate loadChannelInfo/updateSubscribeButton/unsubscribeFromChannel removed.

async function loadChannelMessages(channelId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        console.error('No token for loading channel messages');
        return;
    }
    if (typeof resetChecklistState === 'function') {
        resetChecklistState();
    }

    console.log('loadChannelMessages called for channel:', channelId);

    try {
        const response = await fetch('/api/get-channel-messages', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token,
                channel_id: channelId,
                limit: 50
            })
        });

        const data = await response.json();
        console.log('Channel messages response:', data);

        if (data.success && data.messages) {
            const messagesContainer = document.getElementById('chatMessages');
            if (!messagesContainer) {
                console.error('chatMessages container not found');
                return;
            }

            messagesContainer.innerHTML = '';

            // Отображаем сообщения (обратный порядок для правильного отображения)
            const messages = data.messages.reverse();
            currentChannelMessages = messages;
            channelInfoSelectedMessages.clear();
            console.log('Rendering', messages.length, 'messages');
            let maxLocalId = 0;
            messages.forEach(msg => {
                addChannelMessageToUI(msg);
                // Отмечаем просмотр
                addMessageView(msg.id);
                if (msg.local_id && msg.local_id > maxLocalId) {
                    maxLocalId = msg.local_id;
                }
            });

            // Обновляем read-state для канала
            if (maxLocalId > 0) {
                readChannelState(channelId, maxLocalId);
            }

            // Прокручиваем вниз
            if (messagesContainer) {
                setTimeout(() => {
                    messagesContainer.scrollTop = messagesContainer.scrollHeight;
                }, 100);
            }

            updateChannelInfoPanel();
        } else {
            console.error('Failed to load channel messages:', data);
            notifications.error(data.error || 'Ошибка загрузки сообщений канала');
        }
    } catch (error) {
        console.error('Error loading channel messages:', error);
        notifications.error('Ошибка загрузки сообщений канала');
    }
}

function addChannelMessageToUI(msg) {
    const messagesContainer = document.getElementById('chatMessages');
    if (!messagesContainer) return;

    if (typeof handleChecklistUpdateMessage === 'function' && handleChecklistUpdateMessage(msg)) {
        return;
    }

    const locationPayload = typeof parseLocationPayload === 'function' ? parseLocationPayload(msg.content || '') : null;
    const liveId = msg.message_type === 'live_location' && locationPayload?.liveId ? locationPayload.liveId : null;
    const locationCard = typeof buildLocationCard === 'function' ? buildLocationCard(msg) : null;
    const checklistPayload = typeof parseChecklistPayloadContent === 'function'
        ? parseChecklistPayloadContent(msg.content || '')
        : null;

    if (liveId) {
        const existingLive = messagesContainer.querySelector(`[data-live-id="${liveId}"]`);
        if (existingLive) {
            if (msg.id) {
                existingLive.dataset.messageId = msg.id;
            }
            const timeEl = existingLive.querySelector('.message-time span');
            if (timeEl && msg.time) {
                timeEl.textContent = msg.time;
            }
            const bubble = existingLive.querySelector('.message-bubble');
            if (bubble && locationCard) {
                const existingCard = bubble.querySelector('.message-location');
                if (existingCard) {
                    existingCard.replaceWith(locationCard);
                } else {
                    bubble.appendChild(locationCard);
                }
            }
            return;
        }
    }

    const messageDiv = document.createElement('div');
    messageDiv.className = 'message received'; // В каналах все сообщения отображаются как полученные
    messageDiv.dataset.messageId = msg.id;
    if (liveId) {
        messageDiv.dataset.liveId = liveId;
    }

    const time = msg.time || '';
    const senderName = msg.sender_username || 'Неизвестный';
    const showAuthor = currentChannel?.show_author !== false;

    // Парсим упоминания в сообщении
    const parsedContent = parseMentions(msg.content || '');
    
    const avatarLetter = (senderName || 'X').charAt(0).toUpperCase();
    messageDiv.innerHTML = `
        <div class="message-avatar">${escapeHtml(avatarLetter)}</div>
        <div class="message-bubble">
            ${showAuthor ? `<div class="message-sender" data-user-id="${escapeHtml(msg.sender_id || '')}" data-username="${escapeHtml(senderName)}" style="font-weight:600; margin-bottom:0.35rem; opacity:0.9; cursor:pointer;">${escapeHtml(senderName)}</div>` : ''}
            <div class="message-text">${parsedContent}</div>
            <div class="message-time" style="display:flex; gap:0.5rem; align-items:center;">
                <span>${time}</span>
                ${msg.views_count > 0 ? `<span class="message-views">👁 ${msg.views_count}</span>` : ''}
            </div>
            <div class="message-reactions" id="reactions-${msg.id}"></div>
        </div>
    `;

    // Индикатор закрепления
    if (msg.is_pinned) {
        const pinIndicator = document.createElement('span');
        pinIndicator.className = 'message-pin-indicator';
        pinIndicator.innerHTML = '📌 Закреплено';
        const messageTime = messageDiv.querySelector('.message-time');
        if (messageTime) messageTime.appendChild(pinIndicator);
        messageDiv.classList.add('pinned');
    }

    messagesContainer.appendChild(messageDiv);

    if (typeof applyMessageTtlData === 'function') {
        applyMessageTtlData(messageDiv, msg);
    }
    
    // Добавляем обработчики кликов на упоминания
    const messageText = messageDiv.querySelector('.message-text');
    if (messageText) {
        if (checklistPayload && typeof buildChecklistElement === 'function') {
            const checklistEl = buildChecklistElement(checklistPayload, msg, messageDiv);
            if (checklistEl) {
                messageText.replaceWith(checklistEl);
            }
        } else if (locationCard) {
            messageText.replaceWith(locationCard);
        } else {
            messageText.querySelectorAll('.mention-link').forEach(link => {
                link.addEventListener('click', async (e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    const username = link.dataset.username;
                    if (username) {
                        await handleChannelMentionClick(username);
                    }
                });
            });
        }
    }

    // Клик по имени автора => открыть профиль
    const senderEl = messageDiv.querySelector('.message-sender');
    if (senderEl && typeof window.openUserProfile === 'function') {
        senderEl.addEventListener('click', (e) => {
            e.preventDefault();
            e.stopPropagation();
            const uid = senderEl.dataset.userId || '';
            // For channels, sender_id should be present; fallback to username if not
            if (uid) window.openUserProfile({ user_id: uid });
            else if (senderName) window.openUserProfile({ username: senderName.replace(/^@/, '') });
        });
    }

    // Добавляем обработчик правой кнопки мыши
    messageDiv.addEventListener('contextmenu', (e) => {
        e.preventDefault();
        // Use chat.js context menu if available
        if (typeof handleRightClick === 'function') {
            handleRightClick(e, msg, messageDiv);
        }
    });

    // Загружаем реакции
    loadMessageReactions(msg.id);
}

// Обработка клика на упоминание в канале
async function handleChannelMentionClick(username) {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        notifications.error('Необходима авторизация');
        return;
    }
    
    try {
        // Сначала ищем канал
        const channelResponse = await fetch('/api/search-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ custom_link: username })
        });
        
        const channelData = await channelResponse.json();
        
        if (channelData.success && channelData.channel) {
            // Найден канал - открываем его
            await loadChannels();
            let foundChannel = channels.find(c => c.id === channelData.channel.id);
            
            if (!foundChannel) {
                // Если канала нет в списке, отправляем подписку/заявку
                await subscribeToChannel(channelData.channel.id);
                if (!channelData.channel.is_private) {
                    await loadChannels();
                    foundChannel = channels.find(c => c.id === channelData.channel.id);
                } else {
                    return;
                }
            }
            
            if (foundChannel) {
                selectChannel(foundChannel);
                // Переключаемся на вкладку каналов
                const channelsTab = document.querySelector('[data-tab="channels"]');
                if (channelsTab) {
                    channelsTab.click();
                }
            }
            return;
        }
        
        // Канал не найден - ищем пользователя (если функция доступна из chat.js)
        if (typeof handleMentionClick === 'function') {
            handleMentionClick(username);
        } else {
            notifications.warning('Пользователь или канал не найдены');
        }
    } catch (error) {
        console.error('Error handling mention click:', error);
        notifications.error('Ошибка при обработке упоминания');
    }
}

async function subscribeToChannel(channelId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        const response = await fetch('/api/subscribe-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId })
        });
        
        const data = await response.json();
        if (data.success) {
            const msg = (data.message || '').toLowerCase();
            if (msg.includes('request')) {
                notifications.success('Заявка отправлена');
            } else {
                notifications.success('Подписка на канал успешна');
            }
            // Перезагружаем информацию о канале
            if (currentChannel && currentChannel.id === channelId) {
                await loadChannelInfo(channelId);
            }
            // Перезагружаем список каналов
            await loadChannels();
        } else {
            notifications.error(data.error || 'Ошибка подписки на канал');
        }
    } catch (error) {
        console.error('Subscribe error:', error);
        notifications.error('Ошибка подписки на канал');
    }
}

async function loadMessageReactions(messageId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;

    try {
        const response = await fetch('/api/get-message-reactions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ message_id: messageId })
        });

        const data = await response.json();

        if (data.success && data.reactions) {
            const reactionsContainer = document.getElementById(`reactions-${messageId}`);
            if (!reactionsContainer) return;

            reactionsContainer.innerHTML = '';

            data.reactions.forEach(reaction => {
                const reactionBtn = document.createElement('button');
                reactionBtn.className = 'reaction-btn';
                reactionBtn.textContent = `${reaction.reaction} ${reaction.count}`;
                reactionBtn.addEventListener('click', () => {
                    toggleReaction(messageId, reaction.reaction);
                });
                reactionsContainer.appendChild(reactionBtn);
            });

            // Кнопка добавления реакции
            const addReactionBtn = document.createElement('button');
            addReactionBtn.className = 'reaction-btn add-reaction';
            addReactionBtn.textContent = '+';
            addReactionBtn.title = 'Добавить реакцию';
            addReactionBtn.addEventListener('click', () => {
                showReactionPicker(messageId);
            });
            reactionsContainer.appendChild(addReactionBtn);
        }
    } catch (error) {
        console.error('Error loading reactions:', error);
    }
}

async function toggleReaction(messageId, reaction) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;

    try {
        // Пытаемся удалить реакцию
        let response = await fetch('/api/remove-message-reaction', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token,
                message_id: messageId,
                reaction
            })
        });

        const data = await response.json();

        // Если реакция не была найдена, добавляем её
        if (!data.success) {
            response = await fetch('/api/add-message-reaction', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    token,
                    message_id: messageId,
                    reaction
                })
            });
            const addData = await response.json();
            if (!addData.success) {
                notifications.error(addData.error || 'Не удалось добавить реакцию');
                return;
            }
        }

        // Обновляем реакции
        await loadMessageReactions(messageId);
    } catch (error) {
        console.error('Error toggling reaction:', error);
        notifications.error('Ошибка при добавлении реакции');
    }
}

async function addMessageView(messageId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;

    try {
        await fetch('/api/add-message-view', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token,
                message_id: messageId
            })
        });
    } catch (error) {
        console.error('Error adding view:', error);
    }
}

function showReactionPicker(messageId) {
    // Простой выбор из популярных эмодзи
    const reactions = ['👍', '❤️', '😂', '😮', '😢', '🔥'];
    const picker = document.createElement('div');
    picker.className = 'reaction-picker';
    picker.style.cssText = 'position: absolute; background: var(--black-secondary); padding: 0.5rem; border-radius: 8px; display: flex; gap: 0.5rem; z-index: 1000;';

    reactions.forEach(reaction => {
        const btn = document.createElement('button');
        btn.textContent = reaction;
        btn.style.cssText = 'background: transparent; border: none; font-size: 1.5rem; cursor: pointer; padding: 0.25rem;';
        btn.addEventListener('click', () => {
            toggleReaction(messageId, reaction);
            picker.remove();
        });
        picker.appendChild(btn);
    });

    const reactionsContainer = document.getElementById(`reactions-${messageId}`);
    if (reactionsContainer) {
        reactionsContainer.appendChild(picker);
    }
}

// Отправка сообщения в канал (только для админов)
async function sendChannelMessage(content, messageType = 'text', filePath = '', fileName = '', fileSize = 0, ttlSeconds = 0) {
    if (!currentChannel) return;

    const token = localStorage.getItem('xipher_token');
    if (!token) return;

    try {
        const resolvedTtl = Number.isFinite(ttlSeconds) ? ttlSeconds : 0;
        const requestBody = {
            token,
            channel_id: currentChannel.id,
            content,
            message_type: messageType,
            file_path: filePath,
            file_name: fileName,
            file_size: fileSize
        };
        if (resolvedTtl > 0) {
            requestBody.ttl_seconds = resolvedTtl;
        }
        const response = await fetch('/api/send-channel-message', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(requestBody)
        });

        let data = {};
        try {
            data = await response.json();
        } catch (e) {
            data = {};
        }

        if (data && data.success) {
            // Обновляем сообщения
            await loadChannelMessages(currentChannel.id);
            // Обновляем общий список чатов
            if (typeof loadAllChats === 'function') {
                loadAllChats();
            }
        } else {
            const msg = (data && (data.error || data.message)) || `Ошибка при отправке сообщения${response && !response.ok ? ` (HTTP ${response.status})` : ''}`;
            notifications.error(msg);
            // If token is invalid (server restart or session expired), force re-login
            if (typeof msg === 'string' && msg.toLowerCase().includes('invalid token')) {
                await window.xipherSession?.logout();
                setTimeout(() => {
                    window.location.href = '/login';
                }, 300);
            }
        }
    } catch (error) {
        console.error('Error sending channel message:', error);
        notifications.error('Ошибка при отправке сообщения');
    }
}

async function readChannelState(channelId, maxLocalId) {
    const token = localStorage.getItem('xipher_token');
    if (!token || !channelId || !maxLocalId) return;
    try {
        await fetch('/api/read-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token,
                channel_id: channelId,
                max_read_local: maxLocalId
            })
        });
    } catch (err) {
        console.warn('Failed to update channel read state', err);
    }
}

// Функция для экранирования HTML
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Сохранение настроек каталога для канала
async function saveChannelCatalogSettings(channelId, isPublic, category) {
    try {
        const token = localStorage.getItem('xipher_token');
        const response = await fetch('/api/set-channel-public', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token,
                channel_id: channelId,
                is_public: isPublic ? 'true' : 'false',
                category: category
            })
        });
        
        const data = await response.json();
        if (data.success) {
            if (currentChannel) {
                currentChannel.is_public = isPublic;
                currentChannel.category = category;
            }
            console.log('[Channels] Catalog settings saved');
        } else {
            console.error('[Channels] Failed to save catalog settings:', data.error);
        }
    } catch (e) {
        console.error('[Channels] Error saving catalog settings:', e);
    }
}

// Парсинг упоминаний в тексте сообщения
function parseMentions(text) {
    if (!text) return '';
    
    // Экранируем HTML для безопасности
    const escaped = escapeHtml(text);
    
    // Регулярное выражение для поиска @mentions
    // Формат: @username или @channel (3-50 символов, буквы, цифры, подчеркивания)
    const mentionRegex = /@([a-zA-Z0-9_]{3,50})/g;
    
    return escaped.replace(mentionRegex, (match, username) => {
        return `<span class="mention-link" data-username="${escapeHtml(username)}" style="color: var(--purple-primary); cursor: pointer; text-decoration: underline;">${match}</span>`;
    });
}

// Настройка модального окна настроек канала
function setupChannelSettingsModal() {
    const menuBtn = document.getElementById('channelMenuBtn');
    const settingsModal = document.getElementById('groupChannelSettingsModal');
    
    if (!menuBtn || !settingsModal) return;
    
    // Удаляем старые обработчики если есть
    const newMenuBtn = menuBtn.cloneNode(true);
    menuBtn.parentNode.replaceChild(newMenuBtn, menuBtn);
    
    newMenuBtn.addEventListener('click', () => {
        if (!currentChannel || !currentChannelInfo) {
            notifications.warning('Выберите канал');
            return;
        }
        
        // Показываем настройки канала
        openChannelSettings();
    });
}

// Открытие настроек канала
function openChannelSettings() {
    const settingsModal = document.getElementById('groupChannelSettingsModal');
    if (!hasChannelSettingsAccess()) {
        alert('Access Denied');
        return;
    }
    const newPanel = document.getElementById('channelAdminPanel');
    if (newPanel) {
        openChannelAdminPanel();
        return;
    }
    if (!settingsModal) return;
    
    // Устанавливаем заголовок
    const title = document.getElementById('settingsModalTitle');
    if (title) title.textContent = 'Настройки канала';
    
    // Скрываем настройки группы, показываем настройки канала
    const groupSettings = document.getElementById('groupSettingsContent');
    const channelSettings = document.getElementById('channelSettingsContent');
    if (groupSettings) groupSettings.style.display = 'none';
    if (channelSettings) channelSettings.style.display = 'block';
    
    // Заполняем данные канала
    if (currentChannel) {
        const nameInput = document.getElementById('settingsNameInput');
        const descInput = document.getElementById('settingsDescriptionInput');
        const customLinkInput = document.getElementById('settingsCustomLinkInput');
        const privateCheckbox = document.getElementById('settingsChannelPrivate');
        const showAuthorCheckbox = document.getElementById('settingsChannelShowAuthor');
        const publicCatalogCheckbox = document.getElementById('settingsChannelPublicCatalog');
        const categorySelect = document.getElementById('settingsChannelCategory');
        
        if (nameInput) nameInput.value = currentChannel.name || '';
        if (descInput) descInput.value = currentChannel.description || '';
        if (customLinkInput) customLinkInput.value = currentChannel.custom_link || '';
        if (privateCheckbox) privateCheckbox.checked = currentChannel.is_private || false;
        if (showAuthorCheckbox) showAuthorCheckbox.checked = currentChannel.show_author !== false;
        // Публичный каталог: канал публичный если is_private = false
        if (publicCatalogCheckbox) publicCatalogCheckbox.checked = !currentChannel.is_private;
        if (categorySelect) categorySelect.value = currentChannel.category || '';
        
        // Загружаем разрешенные реакции
        if (typeof loadChannelAllowedReactions === 'function') {
            loadChannelAllowedReactions();
        }
    }
    
    // Показываем модальное окно
    settingsModal.style.display = 'flex';
    
    // Активируем вкладку "Информация"
    const infoTab = document.querySelector('.settings-tab[data-tab="info"]');
    if (infoTab) {
        document.querySelectorAll('.settings-tab').forEach(tab => tab.classList.remove('active'));
        document.querySelectorAll('.settings-tab-content').forEach(content => content.style.display = 'none');
        infoTab.classList.add('active');
        const infoContent = document.getElementById('settingsTabInfo');
        if (infoContent) infoContent.style.display = 'block';
    }
}

// Настройка модального окна редактирования канала (старое, оставляем для совместимости)
function setupEditChannelModal() {
    const modal = document.getElementById('editChannelModal');
    const closeBtn = document.getElementById('closeEditChannelModal');
    const cancelBtn = document.getElementById('cancelEditChannelBtn');
    const confirmBtn = document.getElementById('confirmEditChannelBtn');
    
    if (!modal) return;
    
    closeBtn?.addEventListener('click', () => {
        modal.style.display = 'none';
    });
    
    cancelBtn?.addEventListener('click', () => {
        modal.style.display = 'none';
    });
    
    confirmBtn?.addEventListener('click', async () => {
        if (!currentChannel) return;
        
        const name = document.getElementById('editChannelNameInput').value.trim();
        const description = document.getElementById('editChannelDescriptionInput').value.trim();
        const custom_link = document.getElementById('editChannelCustomLinkInput').value.trim();
        const is_private = document.getElementById('editChannelIsPrivate').checked;
        const show_author = document.getElementById('editChannelShowAuthor').checked;
        
        if (!name || name.length < 3) {
            notifications.error('Название канала должно быть не менее 3 символов');
            return;
        }
        
        await updateChannel(currentChannel.id, name, description, custom_link, is_private, show_author);
        modal.style.display = 'none';
    });
    
    modal?.addEventListener('click', (e) => {
        if (e.target === modal) {
            modal.style.display = 'none';
        }
    });
}

// Поиск в канале
function setupChannelSearch() {
    const searchBtn = document.getElementById('searchChannelBtn');
    
    if (!searchBtn) return;
    
    searchBtn.addEventListener('click', () => {
        if (!currentChannel) {
            notifications.warning('Выберите канал для поиска');
            return;
        }
        
        const query = prompt('Введите текст для поиска в канале:');
        if (query && query.trim()) {
            // Простой поиск по сообщениям
            const messages = document.querySelectorAll('.message');
            let found = false;
            
            messages.forEach(msg => {
                const text = msg.textContent.toLowerCase();
                if (text.includes(query.toLowerCase())) {
                    msg.scrollIntoView({ behavior: 'smooth', block: 'center' });
                    msg.style.backgroundColor = 'rgba(147, 51, 234, 0.2)';
                    setTimeout(() => {
                        msg.style.backgroundColor = '';
                    }, 2000);
                    found = true;
                }
            });
            
            if (!found) {
                notifications.info('Сообщения не найдены');
            }
        }
    });
}

async function updateChannel(channelId, name, description, custom_link, is_private, show_author) {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        notifications.error('Необходима авторизация');
        return;
    }
    
    try {
        // Обновляем название
        let response = await fetch('/api/update-channel-name', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId, new_name: name })
        });
        let data = await response.json();
        if (!data.success) {
            notifications.error('Ошибка обновления названия: ' + (data.error || ''));
            return;
        }
        
        // Обновляем описание (может быть пустым)
        response = await fetch('/api/update-channel-description', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId, new_description: description || '' })
        });
        data = await response.json();
        if (!data.success) {
            notifications.error('Ошибка обновления описания: ' + (data.error || ''));
            return; // Прерываем, если ошибка критична
        }
        
        // Обновляем custom_link если изменился (включая удаление)
        const currentCustomLink = currentChannel.custom_link || '';
        if (custom_link !== currentCustomLink) {
            response = await fetch('/api/set-channel-custom-link', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ 
                    token, 
                    channel_id: channelId, 
                    custom_link: custom_link || '' // Отправляем пустую строку для удаления
                })
            });
            data = await response.json();
            if (!data.success) {
                notifications.error('Ошибка обновления username: ' + (data.error || ''));
                return; // Прерываем, если ошибка критична
            }
        }
        
        // Обновляем приватность
        response = await fetch('/api/set-channel-privacy', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId, is_private })
        });
        data = await response.json();
        if (!data.success) {
            notifications.error('Ошибка обновления приватности: ' + (data.error || ''));
        }
        
        // Обновляем показ автора
        response = await fetch('/api/set-channel-show-author', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: channelId, show_author })
        });
        data = await response.json();
        if (!data.success) {
            notifications.error('Ошибка обновления настройки автора: ' + (data.error || ''));
        }
        
        notifications.success('Канал обновлен');
        
        // Обновляем данные канала
        await loadChannels();
        if (currentChannel) {
            const updatedChannel = channels.find(c => c.id === channelId);
            if (updatedChannel) {
                selectChannel(updatedChannel);
            }
        }
    } catch (error) {
        console.error('Error updating channel:', error);
        notifications.error('Ошибка при обновлении канала');
    }
}

// Загрузка разрешенных реакций канала
async function loadChannelAllowedReactions() {
    const container = document.getElementById('channelAllowedReactions');
    if (!container || !currentChannel) return;
    
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        const response = await fetch('/api/get-channel-allowed-reactions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        
        const data = await response.json();
        if (data.success && data.reactions) {
            container.innerHTML = '';
            if (data.reactions.length === 0) {
                container.innerHTML = '<div style="color: var(--text-muted); font-size: 0.85rem;">Все реакции разрешены</div>';
            } else {
                data.reactions.forEach(reaction => {
                    const span = document.createElement('span');
                    span.textContent = reaction;
                    span.style.cssText = 'font-size: 1.5rem; padding: 0.25rem;';
                    container.appendChild(span);
                });
            }
        } else {
            container.innerHTML = '<div style="color: var(--text-muted); font-size: 0.85rem;">Все реакции разрешены</div>';
        }
    } catch (error) {
        console.error('Error loading allowed reactions:', error);
        container.innerHTML = '<div style="color: var(--text-muted); font-size: 0.85rem;">Ошибка загрузки</div>';
    }
}

// --- Новый UI настроек канала (2025) ---
const CHANNEL_REACTION_CHOICES = ["👍","👎","🔥","❤️","😂","😮","😢","👏","🙏","🚀","❗","❓","⭐","💡","✅","❌","🎉","⚡","🥳","🙌"];
let channelAllowedReactionSet = new Set();

function renderChannelReactionGrid() {
    const grid = document.getElementById('channelAdminReactionGrid');
    if (!grid) return;
    grid.innerHTML = '';
    CHANNEL_REACTION_CHOICES.forEach(reaction => {
        const btn = document.createElement('button');
        btn.textContent = reaction;
        btn.className = channelAllowedReactionSet.size === 0 || channelAllowedReactionSet.has(reaction) ? 'btn-secondary active' : 'btn-secondary';
        btn.style.padding = '10px 12px';
        btn.style.fontSize = '18px';
        btn.addEventListener('click', () => toggleChannelAllowedReaction(reaction));
        grid.appendChild(btn);
    });
}

async function loadChannelAllowedReactions2025() {
    if (!currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const response = await fetch('/api/get-channel-allowed-reactions', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        const data = await response.json();
        if (data.success && Array.isArray(data.reactions)) {
            channelAllowedReactionSet = new Set(data.reactions);
        } else {
            channelAllowedReactionSet = new Set();
        }
    } catch (e) {
        console.error('load reactions failed', e);
        channelAllowedReactionSet = new Set();
    }
    renderChannelReactionGrid();
}

async function toggleChannelAllowedReaction(reaction) {
    if (!currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    const selected = channelAllowedReactionSet.has(reaction);
    try {
        const url = selected ? '/api/remove-allowed-reaction' : '/api/add-allowed-reaction';
        const response = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, reaction })
        });
        const data = await response.json();
        if (data.success) {
            if (selected) channelAllowedReactionSet.delete(reaction);
            else channelAllowedReactionSet.add(reaction);
            renderChannelReactionGrid();
            notifications.success('Настройки реакций обновлены');
        } else {
            notifications.error(data.error || 'Не удалось обновить реакции');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сети при обновлении реакций');
    }
}

async function saveChannelGeneralSettings() {
    if (!currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        notifications.error('Необходима авторизация');
        return;
    }
    const name = document.getElementById('channelAdminName')?.value.trim() || currentChannel.name;
    const description = document.getElementById('channelAdminDescription')?.value.trim() || '';
    const aliasRaw = document.getElementById('channelAdminAlias')?.value.trim() || '';
    const alias = aliasRaw.startsWith('@') ? aliasRaw.slice(1) : aliasRaw;
    const isPrivate = document.getElementById('channelAdminPrivate')?.checked || false;
    const signMessages = document.getElementById('channelAdminSignMessages')?.checked !== false;

    try {
        // name
        const nameResp = await fetch('/api/update-channel-name', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, new_name: name })
        });
        const nameData = await nameResp.json();
        if (!nameData.success) {
            notifications.error(nameData.error || 'Не удалось обновить название');
            return;
        }
        // description
        await fetch('/api/update-channel-description', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, new_description: description })
        });
        // alias
        await fetch('/api/set-channel-custom-link', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, custom_link: alias })
        });
        // privacy
        await fetch('/api/set-channel-privacy', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, is_private: isPrivate ? 'true' : 'false' })
        });
        // sign messages -> reuse show_author toggle
        await fetch('/api/set-channel-show-author', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, show_author: signMessages ? 'true' : 'false' })
        });

        notifications.success('Настройки канала сохранены');
        await loadChannels();
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сохранения настроек');
    }
}

function setChannelInviteToken(channelId, token) {
    if (!channelId) return;
    if (token) {
        channelInviteCache.set(channelId, token);
    } else {
        channelInviteCache.delete(channelId);
    }
}

function getChannelInviteToken(channelId) {
    if (!channelId) return '';
    return channelInviteCache.get(channelId) || '';
}

function updateChannelAdminInvitePreview(token) {
    const preview = document.getElementById('channelAdminInvitePreview');
    if (!preview) return;
    const url = buildChannelInviteUrl(token);
    if (!url) {
        preview.textContent = 'Ссылка не создана';
        preview.dataset.token = '';
        return;
    }
    preview.textContent = url;
    preview.dataset.token = token;
}

function updateChannelAdminAvatarPreview(avatarUrl) {
    const preview = document.getElementById('channelAdminAvatarPreview');
    if (!preview) return;
    preview.innerHTML = '';
    if (avatarUrl) {
        const img = document.createElement('img');
        img.src = avatarUrl;
        img.style.width = '100%';
        img.style.height = '100%';
        img.style.objectFit = 'cover';
        img.style.borderRadius = '20px';
        img.onerror = () => {
            preview.textContent = (currentChannel?.name || 'К').charAt(0).toUpperCase();
        };
        preview.appendChild(img);
    } else {
        preview.textContent = (currentChannel?.name || 'К').charAt(0).toUpperCase();
    }
}

async function uploadChannelAvatar() {
    if (!currentChannel) return;
    const input = document.getElementById('channelAdminAvatarInput');
    const file = input?.files?.[0];
    if (!file) {
        notifications.warning('Выберите файл с изображением');
        return;
    }
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    const formData = new FormData();
    formData.append('token', token);
    formData.append('channel_id', currentChannel.id);
    formData.append('avatar', file, file.name);
    try {
        const response = await fetch('/api/upload-channel-avatar', {
            method: 'POST',
            body: formData
        });
        let data = {};
        try {
            data = await response.json();
        } catch (e) {
            const text = await response.text();
            try {
                data = JSON.parse(text);
            } catch (err) {
                data = {};
            }
        }
        const avatarUrl = data.url || data.avatar_url;
        if (avatarUrl) {
            currentChannel.avatar_url = avatarUrl;
            if (currentChannelInfo?.channel) {
                currentChannelInfo.channel.avatar_url = avatarUrl;
            }
            updateChannelAdminAvatarPreview(avatarUrl);
            updateChannelInfoPanel();
            if (typeof loadAllChats === 'function') {
                loadAllChats();
            }
            notifications.success('Аватар обновлен');
            if (input) input.value = '';
        } else {
            notifications.error(data.error || 'Не удалось обновить аватар');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка загрузки аватара');
    }
}

async function createChannelInvite() {
    if (!currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const response = await fetch('/api/create-channel-invite', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        const data = await response.json();
        if (data.success && data.token) {
            setChannelInviteToken(currentChannel.id, data.token);
            updateChannelAdminInvitePreview(data.token);
            notifications.success('Ссылка создана');
        } else {
            notifications.error(data.error || 'Не удалось создать ссылку');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка создания ссылки');
    }
}

function copyChannelInvite() {
    if (!currentChannel) return;
    const token = getChannelInviteToken(currentChannel.id) ||
        document.getElementById('channelAdminInvitePreview')?.dataset?.token ||
        '';
    const url = buildChannelInviteUrl(token);
    if (!url) {
        notifications.info('Ссылка еще не создана');
        return;
    }
    copyToClipboard(url);
}

async function linkChannelDiscussion() {
    if (!currentChannel) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    const input = document.getElementById('channelAdminDiscussionId');
    const discussionId = input?.value.trim() || '';
    const body = {
        token,
        channel_id: currentChannel.id
    };
    if (discussionId) {
        body.discussion_id = discussionId;
    } else {
        body.create_new = true;
    }
    try {
        const response = await fetch('/api/link-channel-discussion', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        const data = await response.json();
        if (data.success) {
            if (data.discussion_id && input) {
                input.value = data.discussion_id;
            }
            notifications.success('Обсуждение привязано');
        } else {
            notifications.error(data.error || 'Не удалось связать обсуждение');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка связи обсуждения');
    }
}

async function deleteChannel() {
    if (!currentChannel) return;
    const confirmed = confirm('Удалить канал без возможности восстановления?');
    if (!confirmed) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const response = await fetch('/api/delete-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        const data = await response.json();
        if (data.success) {
            notifications.success('Канал удален');
            const modal = document.getElementById('channelAdminPanel');
            if (modal) modal.style.display = 'none';
            if (window.channelsModule?.resetCurrentChannel) {
                window.channelsModule.resetCurrentChannel();
            }
            if (typeof loadAllChats === 'function') {
                loadAllChats();
            }
        } else {
            notifications.error(data.error || 'Не удалось удалить канал');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка удаления канала');
    }
}

function switchChannelTab(tab) {
    document.querySelectorAll('.channel-tab').forEach(el => el.style.display = 'none');
    const target = document.getElementById(`channelTab-${tab}`);
    if (target) target.style.display = 'block';
    document.querySelectorAll('[data-channel-tab]').forEach(btn => {
        btn.classList.toggle('btn-primary', btn.getAttribute('data-channel-tab') === tab);
    });
}

function openChannelAdminPanel() {
    if (!currentChannel) {
        notifications.warning('Выберите канал');
        return;
    }
    const modal = document.getElementById('channelAdminPanel');
    if (!modal) return;
    document.getElementById('channelAdminName').value = currentChannel.name || '';
    document.getElementById('channelAdminDescription').value = currentChannel.description || '';
    document.getElementById('channelAdminAlias').value = currentChannel.custom_link ? `@${currentChannel.custom_link}` : '';
    document.getElementById('channelAdminPrivate').checked = !!currentChannel.is_private;
    document.getElementById('channelAdminSignMessages').checked = currentChannel.show_author !== false;
    const subtitle = document.getElementById('channelAdminPanelSubtitle');
    if (subtitle) subtitle.textContent = `ID: ${currentChannel.id}`;
    const avatarUrl = currentChannel.avatar_url || currentChannelInfo?.channel?.avatar_url || '';
    updateChannelAdminAvatarPreview(avatarUrl);
    updateChannelAdminInvitePreview(getChannelInviteToken(currentChannel.id));
    const discussionInput = document.getElementById('channelAdminDiscussionId');
    if (discussionInput) discussionInput.value = '';
    switchChannelTab('general');
    modal.style.display = 'flex';
    loadChannelAllowedReactions2025();
    loadChannelMembers2025();
}

function setupChannelAdminPanelHandlers() {
    const modal = document.getElementById('channelAdminPanel');
    if (!modal) return;
    const closeBtn = document.getElementById('closeChannelAdminPanel');
    closeBtn?.addEventListener('click', () => modal.style.display = 'none');
    modal.addEventListener('click', (e) => {
        if (e.target === modal) modal.style.display = 'none';
    });
    document.querySelectorAll('[data-channel-tab]').forEach(btn => {
        btn.addEventListener('click', () => switchChannelTab(btn.getAttribute('data-channel-tab')));
    });
    const saveBtn = document.getElementById('channelAdminSaveGeneral');
    saveBtn?.addEventListener('click', saveChannelGeneralSettings);
    const resetBtn = document.getElementById('channelAdminResetReactions');
    resetBtn?.addEventListener('click', async () => {
        channelAllowedReactionSet = new Set();
        renderChannelReactionGrid();
        notifications.info('Теперь разрешены все реакции. Для ограничения выберите эмодзи.');
    });
    const avatarInput = document.getElementById('channelAdminAvatarInput');
    avatarInput?.addEventListener('change', () => {
        const file = avatarInput.files?.[0];
        if (!file) return;
        const url = URL.createObjectURL(file);
        updateChannelAdminAvatarPreview(url);
    });
    const uploadBtn = document.getElementById('channelAdminUploadAvatar');
    uploadBtn?.addEventListener('click', uploadChannelAvatar);
    const inviteBtn = document.getElementById('channelAdminCreateInvite');
    inviteBtn?.addEventListener('click', createChannelInvite);
    const inviteCopyBtn = document.getElementById('channelAdminCopyInvite');
    inviteCopyBtn?.addEventListener('click', copyChannelInvite);
    const linkChatBtn = document.getElementById('channelAdminLinkChat');
    linkChatBtn?.addEventListener('click', linkChannelDiscussion);
    const deleteBtn = document.getElementById('channelAdminDeleteBtn');
    deleteBtn?.addEventListener('click', deleteChannel);
}

function setupChannelAdminPermModalHandlers() {
    const modal = document.getElementById('channelAdminPermModal');
    if (!modal) return;
    const closeBtn = document.getElementById('closeChannelAdminPermModal');
    closeBtn?.addEventListener('click', closeChannelAdminPermModal);
    const cancelBtn = document.getElementById('channelAdminPermCancel');
    cancelBtn?.addEventListener('click', closeChannelAdminPermModal);
    const saveBtn = document.getElementById('channelAdminPermSave');
    saveBtn?.addEventListener('click', saveChannelAdminPerms);
    modal.addEventListener('click', (e) => {
        if (e.target === modal) closeChannelAdminPermModal();
    });
}

async function loadChannelMembers2025() {
    const token = localStorage.getItem('xipher_token');
    if (!token || !currentChannel) return;
    channelSupportsAdminPerms = false;
    const tabs = {
        admins: document.getElementById('channelTab-admins'),
        subscribers: document.getElementById('channelTab-subscribers'),
        blacklist: document.getElementById('channelTab-blacklist'),
        requests: document.getElementById('channelTab-requests')
    };
    const placeholders = {
        admins: 'Список администраторов появится здесь.',
        subscribers: 'Список подписчиков появится здесь.',
        blacklist: 'Заблокированные пользователи будут отображаться здесь.',
        requests: 'Заявки на вступление будут отображаться здесь.'
    };

    // Clear
    Object.entries(tabs).forEach(([key, el]) => {
        if (el) {
            el.innerHTML = `<div class="channel-admin-empty">${placeholders[key]}</div>`;
        }
    });

    try {
        const resp = await fetch('/api/get-channel-members', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        const data = await resp.json();
        if (!data.success || !Array.isArray(data.members)) {
            return;
        }
        channelSupportsAdminPerms = data.members.some(m => m.admin_perms !== undefined);

        const admins = data.members.filter(m => m.role === 'creator' || m.role === 'owner' || m.role === 'admin');
        const subscribers = data.members.filter(m => m.role === 'subscriber' && !m.is_banned);
        const blacklist = data.members.filter(m => m.is_banned);

        renderChannelMemberList(tabs.admins, admins, 'Администраторы не найдены');
        renderChannelMemberList(tabs.subscribers, subscribers, 'Подписчики отсутствуют');
        renderChannelMemberList(tabs.blacklist, blacklist, 'Список блокировок пуст');

        // Join requests
        const reqResp = await fetch('/api/get-channel-join-requests', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id })
        });
        const reqData = await reqResp.json();
        if (reqData.success && Array.isArray(reqData.requests)) {
            renderJoinRequestsList(tabs.requests, reqData.requests);
        }
    } catch (e) {
        console.error('loadChannelMembers2025 error', e);
    }
}

function renderChannelMemberList(container, items, emptyText) {
    if (!container) return;
    if (!items || items.length === 0) {
        container.innerHTML = `<div class="channel-admin-empty">${emptyText}</div>`;
        return;
    }
    const list = document.createElement('div');
    list.style.display = 'flex';
    list.style.flexDirection = 'column';
    list.style.gap = '8px';
    items.forEach(m => {
        const row = document.createElement('div');
        row.style.display = 'flex';
        row.style.alignItems = 'center';
        row.style.justifyContent = 'space-between';
        row.style.padding = '10px 12px';
        row.style.background = 'var(--bg-secondary)';
        row.style.border = '1px solid var(--border-color)';
        row.style.borderRadius = '10px';

        const left = document.createElement('div');
        left.style.display = 'flex';
        left.style.alignItems = 'center';
        left.style.gap = '10px';

        const avatar = document.createElement('div');
        avatar.textContent = (m.username || '?').charAt(0).toUpperCase();
        avatar.style.width = '32px';
        avatar.style.height = '32px';
        avatar.style.borderRadius = '50%';
        avatar.style.display = 'grid';
        avatar.style.placeItems = 'center';
        avatar.style.background = 'linear-gradient(135deg, #7c3aed, #3b82f6)';
        avatar.style.color = '#fff';
        avatar.style.fontWeight = '700';

        const text = document.createElement('div');
        const roleLabel = formatChannelRoleLabel(m.role);
        const metaParts = [roleLabel];
        const adminTitle = (m.admin_title || '').trim();
        if (adminTitle) {
            metaParts.push(`«${adminTitle}»`);
        }
        if (m.role === 'admin' || m.role === 'owner' || m.role === 'creator') {
            const permsLabel = m.role === 'admin'
                ? formatAdminPermsShort(m.admin_perms)
                : 'Полные права';
            if (permsLabel) metaParts.push(permsLabel);
        }
        if (m.is_banned) metaParts.push('заблокирован');
        const metaText = metaParts.map(part => escapeHtml(part)).join(' · ');
        const displayName = m.username || m.user_id || 'user';
        text.innerHTML = `<div style="font-weight:600;">${escapeHtml(displayName)}</div><div style="color:var(--text-secondary);font-size:12px;">${metaText}</div>`;

        left.appendChild(avatar);
        left.appendChild(text);

        row.appendChild(left);
        list.appendChild(row);

        row.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            openChannelMemberContextMenu(e, m);
        });
    });
    container.innerHTML = '';
    container.appendChild(list);
}

function renderJoinRequestsList(container, items) {
    if (!container) return;
    if (!items || items.length === 0) {
        container.innerHTML = '<div class="channel-admin-empty">Заявок нет</div>';
        return;
    }
    const list = document.createElement('div');
    list.style.display = 'flex';
    list.style.flexDirection = 'column';
    list.style.gap = '8px';

    items.forEach(req => {
        const row = document.createElement('div');
        row.style.display = 'flex';
        row.style.alignItems = 'center';
        row.style.justifyContent = 'space-between';
        row.style.padding = '10px 12px';
        row.style.background = 'var(--bg-secondary)';
        row.style.border = '1px solid var(--border-color)';
        row.style.borderRadius = '10px';

        const left = document.createElement('div');
        left.style.display = 'flex';
        left.style.alignItems = 'center';
        left.style.gap = '10px';

        const avatar = document.createElement('div');
        avatar.textContent = (req.username || req.user_id || '?').charAt(0).toUpperCase();
        avatar.style.width = '32px';
        avatar.style.height = '32px';
        avatar.style.borderRadius = '50%';
        avatar.style.display = 'grid';
        avatar.style.placeItems = 'center';
        avatar.style.background = 'linear-gradient(135deg, #10b981, #22c55e)';
        avatar.style.color = '#0b1224';
        avatar.style.fontWeight = '700';

        const text = document.createElement('div');
        text.innerHTML = `<div style="font-weight:600;">${escapeHtml(req.username || 'user')}</div><div style="color:var(--text-secondary);font-size:12px;">${escapeHtml(req.user_id || '')}</div>`;

        left.appendChild(avatar);
        left.appendChild(text);

        row.appendChild(left);

        const actions = document.createElement('div');
        actions.style.display = 'flex';
        actions.style.gap = '8px';

        const acceptBtn = document.createElement('button');
        acceptBtn.className = 'btn-primary btn-small';
        acceptBtn.textContent = 'Принять';
        acceptBtn.addEventListener('click', () => respondJoinRequest(req.user_id, true));

        const rejectBtn = document.createElement('button');
        rejectBtn.className = 'btn-secondary btn-small';
        rejectBtn.textContent = 'Отклонить';
        rejectBtn.addEventListener('click', () => respondJoinRequest(req.user_id, false));

        actions.appendChild(acceptBtn);
        actions.appendChild(rejectBtn);
        row.appendChild(actions);

        list.appendChild(row);
    });

    container.innerHTML = '';
    container.appendChild(list);
}

async function respondJoinRequest(targetUserId, accept) {
    const token = localStorage.getItem('xipher_token');
    if (!token || !currentChannel) return;
    const url = accept ? '/api/accept-channel-join-request' : '/api/reject-channel-join-request';
    try {
        const resp = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, channel_id: currentChannel.id, target_user_id: targetUserId })
        });
        const data = await resp.json();
        if (data.success) {
            notifications.success(accept ? 'Заявка принята' : 'Заявка отклонена');
            loadChannelMembers2025();
        } else {
            notifications.error(data.error || 'Ошибка обработки заявки');
        }
    } catch (e) {
        console.error(e);
        notifications.error('Ошибка сети');
    }
}

// Настройка обработчиков для модального окна настроек канала
function setupChannelSettingsHandlers() {
    const saveInfoBtn = document.getElementById('saveInfoBtn');
    const saveSettingsBtn = document.getElementById('saveSettingsBtn');
    const closeBtn = document.getElementById('closeGroupChannelSettingsModal');
    
    // Обработчик сохранения информации
    if (saveInfoBtn) {
        saveInfoBtn.addEventListener('click', async () => {
            if (!currentChannel || !currentChannelInfo || !currentChannelInfo.is_admin) {
                notifications.error('Только администратор может изменять настройки канала');
                return;
            }
            
            const name = document.getElementById('settingsNameInput')?.value.trim();
            const description = document.getElementById('settingsDescriptionInput')?.value.trim();
            const customLink = document.getElementById('settingsCustomLinkInput')?.value.trim();
            
            if (!name || name.length < 3) {
                notifications.error('Название канала должно быть не менее 3 символов');
                return;
            }
            
            await updateChannel(currentChannel.id, name, description, customLink, 
                document.getElementById('settingsChannelPrivate')?.checked || false,
                document.getElementById('settingsChannelShowAuthor')?.checked !== false);
        });
    }
    
    // Обработчик сохранения настроек
    if (saveSettingsBtn) {
        saveSettingsBtn.addEventListener('click', async () => {
            if (!currentChannel || !currentChannelInfo || !currentChannelInfo.is_admin) {
                notifications.error('Только администратор может изменять настройки канала');
                return;
            }
            
            const isPrivate = document.getElementById('settingsChannelPrivate')?.checked || false;
            const showAuthor = document.getElementById('settingsChannelShowAuthor')?.checked !== false;
            const isPublicCatalog = document.getElementById('settingsChannelPublicCatalog')?.checked || false;
            const category = document.getElementById('settingsChannelCategory')?.value || '';
            
            await updateChannel(currentChannel.id, currentChannel.name, currentChannel.description, 
                currentChannel.custom_link, isPrivate, showAuthor);
            
            // Сохранить настройки каталога
            if (isPublicCatalog || category) {
                await saveChannelCatalogSettings(currentChannel.id, isPublicCatalog, category);
            }
        });
    }
    
    // Обработчик закрытия
    if (closeBtn) {
        closeBtn.addEventListener('click', () => {
            const modal = document.getElementById('groupChannelSettingsModal');
            if (modal) modal.style.display = 'none';
        });
    }
    
    // Закрытие при клике вне модального окна
    const modal = document.getElementById('groupChannelSettingsModal');
    if (modal) {
        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                modal.style.display = 'none';
            }
        });
    }
}

// Инициализация при загрузке страницы
function initChannels() {
    console.log('Initializing channels module...');
    setupCreateChannelModal();
    setupEditChannelModal();
    setupChannelSettingsModal();
    setupChannelSettingsHandlers();
    setupChannelAdminPanelHandlers();
    setupChannelAdminPermModalHandlers();
    setupChannelSearch();
    setupChannelInfoOverlay();
    if (window.currentViewType === 'channels') {
        loadChannels();
    }
}

// Создаем/обновляем модуль после определения всех функций
Object.assign(window.channelsModule, {
    loadChannels: loadChannels,
    selectChannel: selectChannel,
    sendChannelMessage: sendChannelMessage,
    currentChannel: () => currentChannel,
    openChannelInfoPanel: openChannelInfoPanel,
    updateChannelInfoPanel: updateChannelInfoPanel,
    // Channel can be selected from the unified "all" list, so don't depend on currentViewType.
    // We instead rely on currentChannel being set, and chat.js will reset it when switching to other chat types.
    isChannelActive: () => currentChannel !== null,
    resetCurrentChannel: () => {
        currentChannel = null;
        currentChannelInfo = null;
        currentChannelMessages = [];
        channelInfoSelectedMessages.clear();
        updateChannelInfoPanel();
        closeChannelInfoOverlay();
        // Keep UI consistent
        const menuBtn = document.getElementById('channelMenuBtn');
        if (menuBtn) menuBtn.style.display = 'none';
        const searchBtn = document.getElementById('searchChannelBtn');
        if (searchBtn) searchBtn.style.display = 'none';
    },
    channels: () => channels,
    _initialized: true,
    _pending: false
});
console.log('Channels module created successfully');

// Экспортируем для глобального доступа
window.initChannels = initChannels;

// Автоматическая инициализация при загрузке DOM
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initChannels);
} else {
    // Если DOM уже загружен, инициализируем сразу
    initChannels();
}
