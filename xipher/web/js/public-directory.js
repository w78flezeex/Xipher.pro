// ============================================
// Каталог публичных групп и каналов
// Как в Telegram - с категориями и поиском
// ============================================

const PublicDirectory = (function() {
    'use strict';

    // Категории для фильтрации
    const CATEGORIES = [
        { id: 'all', name: 'Все', icon: 'globe', emoji: '🌍' },
        { id: 'trending', name: 'Популярные', icon: 'trending', emoji: '🔥' },
        { id: 'news', name: 'Новости', icon: 'news', emoji: '📰' },
        { id: 'tech', name: 'Технологии', icon: 'tech', emoji: '💻' },
        { id: 'crypto', name: 'Крипто', icon: 'crypto', emoji: '₿' },
        { id: 'gaming', name: 'Игры', icon: 'gaming', emoji: '🎮' },
        { id: 'music', name: 'Музыка', icon: 'music', emoji: '🎵' },
        { id: 'movies', name: 'Кино', icon: 'movies', emoji: '🎬' },
        { id: 'education', name: 'Образование', icon: 'education', emoji: '📚' },
        { id: 'sport', name: 'Спорт', icon: 'sport', emoji: '⚽' },
        { id: 'art', name: 'Искусство', icon: 'art', emoji: '🎨' },
        { id: 'food', name: 'Еда', icon: 'food', emoji: '🍕' },
        { id: 'travel', name: 'Путешествия', icon: 'travel', emoji: '✈️' },
        { id: 'business', name: 'Бизнес', icon: 'business', emoji: '💼' },
        { id: 'health', name: 'Здоровье', icon: 'health', emoji: '❤️' },
        { id: 'science', name: 'Наука', icon: 'science', emoji: '🔬' },
        { id: 'people', name: 'Общение', icon: 'people', emoji: '👥' }
    ];

    let currentCategory = 'all';
    let searchQuery = '';
    let isLoading = false;
    let publicItems = [];

    // Получить иконку категории
    function getCategoryIcon(iconName) {
        if (typeof appIconSvg === 'function') {
            return appIconSvg(iconName);
        }
        const cat = CATEGORIES.find(c => c.id === iconName || c.icon === iconName);
        return cat ? cat.emoji : '📁';
    }

    // Создать модальное окно каталога
    function createDirectoryModal() {
        const existingModal = document.getElementById('publicDirectoryModal');
        if (existingModal) return;

        const modal = document.createElement('div');
        modal.id = 'publicDirectoryModal';
        modal.className = 'modal-overlay directory-modal';
        modal.style.display = 'none';
        modal.innerHTML = `
            <div class="modal-content directory-content">
                <div class="directory-header">
                    <div class="directory-header-top">
                        <div class="directory-title">
                            <span class="directory-title-icon">${getCategoryIcon('compass')}</span>
                            <h2>Каталог</h2>
                        </div>
                        <button class="close-btn directory-close" id="closeDirectoryModal">&times;</button>
                    </div>
                    <div class="directory-search">
                        <span class="directory-search-icon">${typeof appIconSvg === 'function' ? appIconSvg('search') : '🔍'}</span>
                        <input type="text" id="directorySearchInput" placeholder="Поиск групп и каналов..." autocomplete="off">
                        <button class="directory-search-clear" id="directorySearchClear" style="display: none;">&times;</button>
                    </div>
                </div>
                
                <div class="directory-body">
                    <div class="directory-categories" id="directoryCategories">
                        ${renderCategories()}
                    </div>
                    
                    <div class="directory-results" id="directoryResults">
                        <div class="directory-loading" id="directoryLoading" style="display: none;">
                            <div class="directory-spinner"></div>
                            <span>Загрузка...</span>
                        </div>
                        <div class="directory-empty" id="directoryEmpty" style="display: none;">
                            <div class="directory-empty-icon">🔍</div>
                            <h3>Ничего не найдено</h3>
                            <p>Попробуйте изменить запрос или выберите другую категорию</p>
                        </div>
                        <div class="directory-list" id="directoryList"></div>
                    </div>
                    
                    <!-- Блок информации о верификации -->
                    <div class="directory-verify-info" id="directoryVerifyInfo">
                        <div class="verify-info-header">
                            <span class="verify-badge-icon">✓</span>
                            <span>Верификация каналов</span>
                        </div>
                        <p class="verify-info-desc">Подтверждённые каналы отмечены галочкой и проверены командой Xipher</p>
                        <button class="verify-info-btn" id="showVerifyRequirements">Как получить?</button>
                    </div>
                </div>
            </div>
        `;

        document.body.appendChild(modal);
        setupDirectoryEvents();
    }

    // Рендер категорий
    function renderCategories() {
        return CATEGORIES.map(cat => `
            <button class="directory-category ${cat.id === currentCategory ? 'active' : ''}" 
                    data-category="${cat.id}">
                <span class="directory-category-icon">${getCategoryIcon(cat.icon)}</span>
                <span class="directory-category-name">${cat.name}</span>
            </button>
        `).join('');
    }

    // Рендер элемента каталога (группа/канал)
    function renderDirectoryItem(item) {
        const isChannel = item.type === 'channel';
        const typeIcon = isChannel ? (typeof appIconSvg === 'function' ? appIconSvg('channel') : '📢') : (typeof appIconSvg === 'function' ? appIconSvg('group') : '👥');
        const verifiedBadge = item.verified ? `<span class="directory-verified" title="Подтверждённый">${typeof appIconSvg === 'function' ? appIconSvg('verified') : '✓'}</span>` : '';
        
        const membersText = formatMembersCount(item.members_count || 0);
        const categoryBadge = item.category && item.category !== 'all' 
            ? `<span class="directory-item-category">${getCategoryEmoji(item.category)}</span>` 
            : '';

        const avatarContent = item.avatar_url 
            ? `<img src="${item.avatar_url}" alt="${escapeHtml(item.name)}" onerror="this.style.display='none'; this.nextElementSibling.style.display='flex';">
               <span class="directory-avatar-letter" style="display: none;">${item.name.charAt(0).toUpperCase()}</span>`
            : `<span class="directory-avatar-letter">${item.name.charAt(0).toUpperCase()}</span>`;

        return `
            <div class="directory-item" data-id="${item.id}" data-type="${item.type}">
                <div class="directory-item-avatar ${isChannel ? 'is-channel' : 'is-group'}">
                    ${avatarContent}
                    <span class="directory-item-type-badge">${typeIcon}</span>
                </div>
                <div class="directory-item-info">
                    <div class="directory-item-header">
                        <span class="directory-item-name">${escapeHtml(item.name)}</span>
                        ${verifiedBadge}
                        ${categoryBadge}
                    </div>
                    <div class="directory-item-meta">
                        <span class="directory-item-members">${typeof appIconSvg === 'function' ? appIconSvg('members') : '👥'} ${membersText}</span>
                        ${item.username ? `<span class="directory-item-username">@${escapeHtml(item.username)}</span>` : ''}
                    </div>
                    ${item.description ? `<p class="directory-item-desc">${escapeHtml(item.description.substring(0, 120))}${item.description.length > 120 ? '...' : ''}</p>` : ''}
                </div>
                <button class="directory-item-join btn-primary" data-id="${item.id}" data-type="${item.type}">
                    ${item.is_member ? 'Открыть' : 'Вступить'}
                </button>
            </div>
        `;
    }

    // Форматирование количества участников
    function formatMembersCount(count) {
        if (count >= 1000000) {
            return (count / 1000000).toFixed(1).replace(/\.0$/, '') + 'M';
        }
        if (count >= 1000) {
            return (count / 1000).toFixed(1).replace(/\.0$/, '') + 'K';
        }
        return count.toString();
    }

    // Получить эмодзи категории
    function getCategoryEmoji(categoryId) {
        const cat = CATEGORIES.find(c => c.id === categoryId);
        return cat ? cat.emoji : '';
    }

    // Экранирование HTML
    function escapeHtml(text) {
        if (!text) return '';
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    // Настройка событий
    function setupDirectoryEvents() {
        const modal = document.getElementById('publicDirectoryModal');
        if (!modal) return;

        // Закрытие модального окна
        const closeBtn = document.getElementById('closeDirectoryModal');
        closeBtn?.addEventListener('click', closeDirectory);

        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                closeDirectory();
            }
        });

        // Поиск
        const searchInput = document.getElementById('directorySearchInput');
        const searchClear = document.getElementById('directorySearchClear');
        
        let searchTimeout;
        searchInput?.addEventListener('input', (e) => {
            const query = e.target.value.trim();
            searchClear.style.display = query ? 'flex' : 'none';
            
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(() => {
                searchQuery = query;
                loadPublicItems();
            }, 300);
        });

        searchClear?.addEventListener('click', () => {
            searchInput.value = '';
            searchClear.style.display = 'none';
            searchQuery = '';
            loadPublicItems();
        });

        // Категории
        const categoriesContainer = document.getElementById('directoryCategories');
        categoriesContainer?.addEventListener('click', (e) => {
            const categoryBtn = e.target.closest('.directory-category');
            if (!categoryBtn) return;

            const category = categoryBtn.dataset.category;
            if (category === currentCategory) return;

            currentCategory = category;
            
            // Обновить активную категорию
            document.querySelectorAll('.directory-category').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.category === category);
            });

            loadPublicItems();
        });

        // Клик по элементу (присоединение)
        const resultsList = document.getElementById('directoryList');
        resultsList?.addEventListener('click', (e) => {
            const joinBtn = e.target.closest('.directory-item-join');
            const itemDiv = e.target.closest('.directory-item');
            
            if (joinBtn) {
                const id = joinBtn.dataset.id;
                const type = joinBtn.dataset.type;
                const isMember = joinBtn.textContent.trim() === 'Открыть';
                
                if (isMember) {
                    openExistingChat(id, type);
                } else {
                    joinPublicItem(id, type);
                }
            } else if (itemDiv) {
                // Клик по всей карточке - показать превью
                showItemPreview(itemDiv.dataset.id, itemDiv.dataset.type);
            }
        });

        // ESC для закрытия
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && modal.style.display === 'flex') {
                closeDirectory();
            }
        });

        // Кнопка "Как получить верификацию?"
        const verifyBtn = document.getElementById('showVerifyRequirements');
        verifyBtn?.addEventListener('click', showVerificationRequirements);
    }

    // Показать требования для верификации
    function showVerificationRequirements() {
        // Удалить старый если есть
        const existing = document.getElementById('verifyRequirementsModal');
        if (existing) existing.remove();

        const modal = document.createElement('div');
        modal.id = 'verifyRequirementsModal';
        modal.className = 'modal-overlay verify-modal';
        modal.innerHTML = `
            <div class="modal-content verify-content">
                <div class="verify-modal-header">
                    <div class="verify-modal-badge">✓</div>
                    <h2>Верификация канала</h2>
                    <button class="close-btn" id="closeVerifyModal">&times;</button>
                </div>
                
                <div class="verify-scrollable">
                    <p class="verify-modal-subtitle">
                        Верифицированные каналы получают синюю галочку и отображаются выше в каталоге
                    </p>
                    
                    <div class="verify-requirements">
                        <h3>Требования</h3>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">👥</span>
                            <div class="verify-req-text">
                                <strong>От 1 000 подписчиков</strong>
                                <span>Канал должен иметь активную аудиторию</span>
                            </div>
                        </div>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">📊</span>
                            <div class="verify-req-text">
                                <strong>Живая аудитория</strong>
                                <span>Реальные подписчики, без накрутки</span>
                            </div>
                        </div>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">⭐</span>
                            <div class="verify-req-text">
                                <strong>Премиум-подписка</strong>
                                <span>Владелец канала должен иметь Xipher Premium</span>
                            </div>
                        </div>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">📝</span>
                            <div class="verify-req-text">
                                <strong>Регулярный контент</strong>
                                <span>Публикации минимум 2-3 раза в неделю</span>
                            </div>
                        </div>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">🔗</span>
                            <div class="verify-req-text">
                                <strong>Уникальный юзернейм</strong>
                                <span>Короткая и запоминающаяся ссылка на канал</span>
                            </div>
                        </div>
                        
                        <div class="verify-req-item">
                            <span class="verify-req-icon">✅</span>
                            <div class="verify-req-text">
                                <strong>Соответствие правилам</strong>
                                <span>Отсутствие нарушений правил платформы</span>
                            </div>
                        </div>
                    </div>
                    
                    <div class="verify-apply-section">
                        <h3>Подать заявку</h3>
                        <p class="verify-apply-note">Только владелец канала может подать заявку на верификацию</p>
                        
                        <div class="verify-form">
                            <div class="verify-form-group">
                                <label for="verifyChannelUsername">Юзернейм канала</label>
                                <div class="verify-input-wrapper">
                                    <span class="verify-input-prefix">@</span>
                                    <input type="text" id="verifyChannelUsername" placeholder="channel" maxlength="50">
                                </div>
                            </div>
                            
                            <div class="verify-form-group">
                                <label for="verifyReason">Почему ваш канал заслуживает верификации?</label>
                                <textarea id="verifyReason" placeholder="Расскажите о вашем канале, его тематике и аудитории..." rows="3" maxlength="500"></textarea>
                            </div>
                            
                            <button class="verify-submit-btn" id="submitVerifyRequest">
                                <span class="verify-submit-icon">✓</span>
                                Отправить заявку
                            </button>
                            
                            <div class="verify-form-status" id="verifyFormStatus" style="display: none;"></div>
                        </div>
                    </div>
                    
                    <div class="verify-my-requests" id="verifyMyRequests" style="display: none;">
                        <h3>Мои заявки</h3>
                        <div class="verify-requests-list" id="verifyRequestsList"></div>
                    </div>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
        modal.style.display = 'flex';
        
        // Загрузить мои заявки
        loadMyVerificationRequests();
        
        // Закрытие
        const closeBtn = document.getElementById('closeVerifyModal');
        
        const closeModal = () => {
            modal.style.display = 'none';
            modal.remove();
        };
        
        closeBtn?.addEventListener('click', closeModal);
        modal.addEventListener('click', (e) => {
            if (e.target === modal) closeModal();
        });
        
        // Отправка заявки
        const submitBtn = document.getElementById('submitVerifyRequest');
        submitBtn?.addEventListener('click', submitVerificationRequest);
    }

    // Отправить заявку на верификацию
    async function submitVerificationRequest() {
        const usernameInput = document.getElementById('verifyChannelUsername');
        const reasonInput = document.getElementById('verifyReason');
        const statusDiv = document.getElementById('verifyFormStatus');
        const submitBtn = document.getElementById('submitVerifyRequest');
        
        const username = usernameInput?.value.trim();
        const reason = reasonInput?.value.trim();
        
        if (!username) {
            showVerifyStatus('error', 'Введите юзернейм канала');
            return;
        }
        
        submitBtn.disabled = true;
        submitBtn.innerHTML = '<span class="verify-submit-icon">⏳</span> Отправка...';
        
        try {
            const token = localStorage.getItem('xipher_token');
            const response = await fetch('/api/request-verification', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token,
                    channel_username: username,
                    reason
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                showVerifyStatus('success', 'Заявка успешно отправлена! Рассмотрение занимает до 7 дней.');
                usernameInput.value = '';
                reasonInput.value = '';
                loadMyVerificationRequests();
            } else {
                showVerifyStatus('error', data.message || 'Ошибка при отправке заявки');
            }
        } catch (error) {
            showVerifyStatus('error', 'Ошибка сети');
        } finally {
            submitBtn.disabled = false;
            submitBtn.innerHTML = '<span class="verify-submit-icon">✓</span> Отправить заявку';
        }
    }
    
    function showVerifyStatus(type, message) {
        const statusDiv = document.getElementById('verifyFormStatus');
        if (!statusDiv) return;
        
        statusDiv.className = `verify-form-status ${type}`;
        statusDiv.textContent = message;
        statusDiv.style.display = 'block';
        
        if (type === 'success') {
            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 5000);
        }
    }
    
    // Загрузить мои заявки
    async function loadMyVerificationRequests() {
        const container = document.getElementById('verifyMyRequests');
        const list = document.getElementById('verifyRequestsList');
        
        if (!container || !list) return;
        
        try {
            const token = localStorage.getItem('xipher_token');
            const response = await fetch('/api/get-my-verification-requests', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token })
            });
            
            const data = await response.json();
            
            if (data.success && data.requests && data.requests.length > 0) {
                container.style.display = 'block';
                list.innerHTML = data.requests.map(req => {
                    const statusClass = req.status === 'pending' ? 'pending' : 
                                       req.status === 'approved' ? 'approved' : 'rejected';
                    const statusText = req.status === 'pending' ? '⏳ На рассмотрении' : 
                                      req.status === 'approved' ? '✅ Одобрено' : '❌ Отклонено';
                    const dateStr = new Date(req.created_at).toLocaleDateString('ru-RU');
                    
                    return `
                        <div class="verify-request-item ${statusClass}">
                            <div class="verify-request-header">
                                <span class="verify-request-channel">@${escapeHtml(req.channel_username)}</span>
                                <span class="verify-request-status">${statusText}</span>
                            </div>
                            <div class="verify-request-info">
                                <span class="verify-request-name">${escapeHtml(req.channel_name)}</span>
                                <span class="verify-request-date">${dateStr}</span>
                            </div>
                            ${req.admin_comment ? `<div class="verify-request-comment">💬 ${escapeHtml(req.admin_comment)}</div>` : ''}
                        </div>
                    `;
                }).join('');
            } else {
                container.style.display = 'none';
            }
        } catch (error) {
            console.error('Error loading verification requests:', error);
        }
    }

    // Открыть каталог
    function openDirectory() {
        createDirectoryModal();
        const modal = document.getElementById('publicDirectoryModal');
        if (modal) {
            modal.style.display = 'flex';
            document.body.style.overflow = 'hidden';
            
            // Сбросить состояние
            currentCategory = 'all';
            searchQuery = '';
            
            const searchInput = document.getElementById('directorySearchInput');
            if (searchInput) {
                searchInput.value = '';
            }
            
            // Обновить категории
            const categoriesContainer = document.getElementById('directoryCategories');
            if (categoriesContainer) {
                categoriesContainer.innerHTML = renderCategories();
            }
            
            // Загрузить данные
            loadPublicItems();
            
            // Фокус на поиск
            setTimeout(() => searchInput?.focus(), 100);
        }
    }

    // Закрыть каталог
    function closeDirectory() {
        const modal = document.getElementById('publicDirectoryModal');
        if (modal) {
            modal.style.display = 'none';
            document.body.style.overflow = '';
        }
    }

    // Загрузить публичные группы/каналы
    async function loadPublicItems() {
        if (isLoading) return;
        isLoading = true;

        const loadingEl = document.getElementById('directoryLoading');
        const emptyEl = document.getElementById('directoryEmpty');
        const listEl = document.getElementById('directoryList');

        if (loadingEl) loadingEl.style.display = 'flex';
        if (emptyEl) emptyEl.style.display = 'none';
        if (listEl) listEl.innerHTML = '';

        try {
            const token = localStorage.getItem('xipher_token');
            
            const response = await fetch('/api/public-directory', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    token,
                    category: currentCategory,
                    search: searchQuery,
                    limit: 50
                })
            });

            const data = await response.json();

            if (data.success && data.items) {
                publicItems = data.items;
                renderResults(publicItems);
            } else {
                // Показать демо-данные если API не работает
                publicItems = generateDemoItems();
                renderResults(publicItems);
            }
        } catch (error) {
            console.error('Error loading public directory:', error);
            // Показать демо-данные при ошибке
            publicItems = generateDemoItems();
            renderResults(publicItems);
        } finally {
            isLoading = false;
            if (loadingEl) loadingEl.style.display = 'none';
        }
    }

    // Генерация демо-данных
    function generateDemoItems() {
        const demoItems = [
            { id: 'demo1', type: 'channel', name: 'Xipher News', username: 'xiphernews', description: 'Официальный канал новостей Xipher. Все обновления и анонсы.', members_count: 125000, category: 'news', verified: true },
            { id: 'demo2', type: 'channel', name: 'Tech Updates', username: 'techupdates', description: 'Последние новости из мира технологий, IT и стартапов.', members_count: 89500, category: 'tech' },
            { id: 'demo3', type: 'group', name: 'Crypto Traders', username: 'cryptotraders', description: 'Обсуждение криптовалют, трейдинга и инвестиций.', members_count: 45200, category: 'crypto' },
            { id: 'demo4', type: 'channel', name: 'GameDev', username: 'gamedevrus', description: 'Всё о разработке игр, Unity, Unreal Engine и геймдизайне.', members_count: 67800, category: 'gaming' },
            { id: 'demo5', type: 'group', name: 'Музыкальный чат', username: 'musicchat', description: 'Делимся музыкой, обсуждаем артистов и альбомы.', members_count: 23400, category: 'music' },
            { id: 'demo6', type: 'channel', name: 'Science Daily', username: 'sciencedaily', description: 'Научные открытия, исследования и интересные факты.', members_count: 156000, category: 'science', verified: true },
            { id: 'demo7', type: 'group', name: 'Путешественники', username: 'travelers', description: 'Обмен опытом путешествий, советы и рекомендации.', members_count: 34500, category: 'travel' },
            { id: 'demo8', type: 'channel', name: 'Cinema Club', username: 'cinemaclub', description: 'Обзоры фильмов, сериалов, новинки кинопроката.', members_count: 98700, category: 'movies' },
            { id: 'demo9', type: 'group', name: 'Фитнес и ЗОЖ', username: 'fitnessclub', description: 'Здоровый образ жизни, тренировки, питание.', members_count: 56300, category: 'health' },
            { id: 'demo10', type: 'channel', name: 'Business Insider', username: 'businessinsider', description: 'Бизнес-идеи, стартапы, предпринимательство.', members_count: 178000, category: 'business', verified: true },
            { id: 'demo11', type: 'group', name: 'Художники', username: 'artists', description: 'Творческое сообщество художников и иллюстраторов.', members_count: 28900, category: 'art' },
            { id: 'demo12', type: 'channel', name: 'Рецепты каждый день', username: 'dailyrecipes', description: 'Вкусные и простые рецепты на каждый день.', members_count: 234000, category: 'food' },
            { id: 'demo13', type: 'group', name: 'Образовательный хаб', username: 'eduhub', description: 'Курсы, обучение, саморазвитие и полезные материалы.', members_count: 67800, category: 'education' },
            { id: 'demo14', type: 'channel', name: 'Sport News', username: 'sportnews', description: 'Спортивные новости, результаты матчей, аналитика.', members_count: 145000, category: 'sport' },
            { id: 'demo15', type: 'group', name: 'Чат общения', username: 'talkchat', description: 'Просто общаемся на разные темы, знакомимся.', members_count: 89000, category: 'people' }
        ];

        // Фильтрация по категории
        let filtered = demoItems;
        if (currentCategory !== 'all') {
            filtered = demoItems.filter(item => item.category === currentCategory);
        }

        // Фильтрация по поиску
        if (searchQuery) {
            const query = searchQuery.toLowerCase();
            filtered = filtered.filter(item => 
                item.name.toLowerCase().includes(query) ||
                (item.username && item.username.toLowerCase().includes(query)) ||
                (item.description && item.description.toLowerCase().includes(query))
            );
        }

        // Сортировка: trending показывает по количеству участников
        if (currentCategory === 'trending') {
            filtered = [...demoItems].sort((a, b) => b.members_count - a.members_count).slice(0, 10);
        }

        return filtered;
    }

    // Рендер результатов
    function renderResults(items) {
        const listEl = document.getElementById('directoryList');
        const emptyEl = document.getElementById('directoryEmpty');

        if (!listEl) return;

        if (items.length === 0) {
            listEl.innerHTML = '';
            if (emptyEl) emptyEl.style.display = 'flex';
            return;
        }

        if (emptyEl) emptyEl.style.display = 'none';
        listEl.innerHTML = items.map(item => renderDirectoryItem(item)).join('');
    }

    // Присоединиться к группе/каналу
    async function joinPublicItem(id, type) {
        const token = localStorage.getItem('xipher_token');
        if (!token) {
            if (typeof notifications !== 'undefined') {
                notifications.error('Необходима авторизация');
            }
            return;
        }

        const button = document.querySelector(`.directory-item-join[data-id="${id}"]`);
        if (button) {
            button.disabled = true;
            button.textContent = 'Вступаем...';
        }

        try {
            const endpoint = type === 'channel' ? '/api/subscribe-channel' : '/api/join-group';
            const bodyKey = type === 'channel' ? 'channel_id' : 'group_id';

            const response = await fetch(endpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    token,
                    [bodyKey]: id
                })
            });

            const data = await response.json();

            if (data.success) {
                if (typeof notifications !== 'undefined') {
                    notifications.success(type === 'channel' ? 'Вы подписались на канал' : 'Вы вступили в группу');
                }
                
                if (button) {
                    button.textContent = 'Открыть';
                    button.disabled = false;
                }

                // Обновить список чатов
                if (typeof loadAllChats === 'function') {
                    loadAllChats();
                }

                // Закрыть каталог и открыть чат
                closeDirectory();
                openExistingChat(id, type);
            } else {
                throw new Error(data.error || 'Ошибка');
            }
        } catch (error) {
            console.error('Error joining:', error);
            if (typeof notifications !== 'undefined') {
                notifications.error('Не удалось вступить: ' + (error.message || 'Попробуйте позже'));
            }
            if (button) {
                button.disabled = false;
                button.textContent = 'Вступить';
            }
        }
    }

    // Открыть существующий чат
    function openExistingChat(id, type) {
        closeDirectory();

        if (type === 'channel') {
            if (typeof window.channelsModule !== 'undefined' && typeof window.channelsModule.selectChannel === 'function') {
                window.channelsModule.selectChannel({ id });
            }
        } else {
            if (typeof selectGroup === 'function') {
                selectGroup({ id });
            }
        }
    }

    // Показать превью элемента
    function showItemPreview(id, type) {
        const item = publicItems.find(i => i.id === id);
        if (!item) return;

        // Можно добавить модальное окно с детальной информацией
        // Пока просто присоединяемся
        const button = document.querySelector(`.directory-item-join[data-id="${id}"]`);
        if (button) {
            button.click();
        }
    }

    // Публичный API
    return {
        open: openDirectory,
        close: closeDirectory,
        refresh: loadPublicItems,
        CATEGORIES
    };
})();

// Экспорт в глобальный объект
if (typeof window !== 'undefined') {
    window.PublicDirectory = PublicDirectory;
}

// Добавить кнопку в боковую панель при загрузке
document.addEventListener('DOMContentLoaded', () => {
    // Ждём загрузки страницы
    setTimeout(() => {
        const searchSection = document.getElementById('chatsSearchSection');
        if (searchSection && !document.getElementById('openDirectoryBtn')) {
            // Добавляем кнопку каталога
            const directoryBtn = document.createElement('button');
            directoryBtn.id = 'openDirectoryBtn';
            directoryBtn.className = 'directory-open-btn';
            directoryBtn.title = 'Каталог публичных групп и каналов';
            directoryBtn.innerHTML = `
                <span class="directory-btn-icon">${typeof appIconSvg === 'function' ? appIconSvg('compass') : '🧭'}</span>
                <span class="directory-btn-text">Каталог</span>
            `;
            directoryBtn.addEventListener('click', () => PublicDirectory.open());
            
            // Вставляем после поля поиска
            searchSection.appendChild(directoryBtn);
        }
    }, 500);
});
