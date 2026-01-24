// =====================================================
// Xipher Topics Module - Telegram-style Forum Topics
// Full implementation matching Telegram 2026 features
// =====================================================

// Helper functions for browser compatibility
function safeGet(obj, ...keys) {
    let result = obj;
    for (const key of keys) {
        if (result == null) return undefined;
        result = result[key];
    }
    return result;
}

function safeCall(obj, method, ...args) {
    if (obj && typeof obj[method] === 'function') {
        return obj[method](...args);
    }
    return undefined;
}

// State
let currentTopics = [];
let currentTopic = null;
let currentTopicMessages = [];
let forumModeGroups = new Map();
let topicNotificationSettings = new Map();
let viewAsMessages = false;
let topicDraftMessages = new Map();
let topicReplyToMessage = null;
let pinnedTopicMessages = new Map();
let unreadCounts = new Map();
let topicUIMode = 'list';
let searchQuery = '';
let isLoadingMore = false;
let hasMoreMessages = true;
let currentGroupId = null;

// Constants
const MAX_PINNED_TOPICS = 5;
const MESSAGES_PER_PAGE = 50;

const TOPIC_COLORS = [
    '#6fb1fc', '#ffa44e', '#ff7a82', '#b691ff',
    '#ffbc5c', '#7ed7a8', '#ff87b0', '#8fd3ff'
];

const TOPIC_EMOJIS = [
    '💬', '📢', '❓', '💡', '🎯', '📌', '🔥', '⚡',
    '📚', '🎨', '🎮', '💻', '📷', '🎵', '🎬', '🏆',
    '✨', '💎', '🌟', '🚀', '🎁', '💪', '👋', '🔔',
    '📝', '💼', '🛠️', '🎓', '🌍', '💰', '📊', '🎪'
];

// =====================================================
// API Functions
// =====================================================

// =====================================================
// Topic Tabs - Horizontal tabs above messages
// =====================================================

function renderTopicTabs(topics, groupId) {
    // Remove existing tabs
    hideTopicTabs();
    
    if (!topics || topics.length === 0) return;
    
    // Find chat content area to insert tabs above messages
    const chatContent = document.querySelector('.chat-content');
    if (!chatContent) return;
    
    // Create tabs container
    const tabsContainer = document.createElement('div');
    tabsContainer.className = 'topic-tabs-container';
    tabsContainer.id = 'topicTabsContainer';
    tabsContainer.innerHTML = `
        <div class="topic-tabs-scroll">
            <div class="topic-tabs" id="topicTabs">
                ${topics.map(topic => `
                    <button class="topic-tab${topic.is_general ? ' general' : ''}" 
                            data-topic-id="${topic.id}"
                            onclick="window.openTopicTab('${topic.id}')">
                        <span class="topic-tab-icon" style="background: ${topic.icon_color || '#6fb1fc'}">${topic.icon_emoji || '💬'}</span>
                        <span class="topic-tab-name">${escapeHtml(topic.name)}</span>
                        ${topic.unread_count > 0 ? `<span class="topic-tab-badge">${topic.unread_count}</span>` : ''}
                    </button>
                `).join('')}
            </div>
            <button class="topic-tab-add" onclick="window.showCreateTopicModal('${groupId}')" title="Новая тема">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
                    <path d="M12 5v14M5 12h14" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                </svg>
            </button>
        </div>
    `;
    
    // Add styles if not exists
    if (!document.getElementById('topicTabsStyles')) {
        const styles = document.createElement('style');
        styles.id = 'topicTabsStyles';
        styles.textContent = `
            .topic-tabs-container {
                background: var(--bg-secondary, #1a1a1a);
                border-bottom: 1px solid var(--border-color, #333);
                padding: 8px 12px;
                flex-shrink: 0;
            }
            .topic-tabs-scroll {
                display: flex;
                align-items: center;
                gap: 8px;
            }
            .topic-tabs {
                display: flex;
                gap: 6px;
                overflow-x: auto;
                scrollbar-width: none;
                -ms-overflow-style: none;
                flex: 1;
            }
            .topic-tabs::-webkit-scrollbar {
                display: none;
            }
            .topic-tab {
                display: flex;
                align-items: center;
                gap: 6px;
                padding: 6px 12px;
                border-radius: 20px;
                border: none;
                background: var(--bg-tertiary, #2a2a2a);
                color: var(--text-secondary, #aaa);
                cursor: pointer;
                white-space: nowrap;
                font-size: 13px;
                transition: all 0.2s ease;
            }
            .topic-tab:hover {
                background: var(--bg-hover, #333);
                color: var(--text-primary, #fff);
            }
            .topic-tab.active {
                background: var(--accent, #229ED9);
                color: white;
            }
            .topic-tab.general {
                font-weight: 600;
            }
            .topic-tab-icon {
                width: 20px;
                height: 20px;
                border-radius: 50%;
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 11px;
            }
            .topic-tab-name {
                max-width: 120px;
                overflow: hidden;
                text-overflow: ellipsis;
            }
            .topic-tab-badge {
                background: #ff4444;
                color: white;
                font-size: 10px;
                padding: 2px 6px;
                border-radius: 10px;
                font-weight: 600;
            }
            .topic-tab-add {
                width: 32px;
                height: 32px;
                border-radius: 50%;
                border: none;
                background: var(--accent, #229ED9);
                color: white;
                cursor: pointer;
                display: flex;
                align-items: center;
                justify-content: center;
                flex-shrink: 0;
                transition: transform 0.2s;
            }
            .topic-tab-add:hover {
                transform: scale(1.1);
            }
        `;
        document.head.appendChild(styles);
    }
    
    // Insert before chat messages
    const chatMessages = document.getElementById('chatMessages');
    if (chatMessages) {
        chatMessages.parentNode.insertBefore(tabsContainer, chatMessages);
    } else {
        chatContent.insertBefore(tabsContainer, chatContent.firstChild);
    }
    
    // Store topics for later use
    currentTopics = topics;
    currentGroupId = groupId;
}

function hideTopicTabs() {
    const container = document.getElementById('topicTabsContainer');
    if (container) container.remove();
}

function updateActiveTab(topicId) {
    document.querySelectorAll('.topic-tab').forEach(tab => {
        tab.classList.toggle('active', tab.dataset.topicId === topicId);
    });
}

// Global function for onclick
window.openTopicTab = async function(topicId) {
    updateActiveTab(topicId);
    await openTopic(topicId);
};

async function setGroupForumMode(groupId, enabled) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/set-group-forum-mode', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, group_id: groupId, enabled: enabled.toString() })
        });
        const data = await response.json();
        
        if (data.success) {
            forumModeGroups.set(groupId, enabled);
            notifications.success(enabled ? 'Режим форума включен' : 'Режим форума выключен');
        }
        return data;
    } catch (error) {
        console.error('Error setting forum mode:', error);
        return { success: false, error: error.message };
    }
}

async function loadGroupTopics(groupId, query = '') {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, topics: [] };
    
    currentGroupId = groupId;
    
    try {
        const response = await fetch('/api/get-group-topics', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, group_id: groupId, query })
        });
        const data = await response.json();
        
        if (data.success) {
            currentTopics = data.topics || [];
            forumModeGroups.set(groupId, data.forum_mode);
            
            // Ensure General topic exists
            const hasGeneral = currentTopics.some(t => t.is_general);
            if (!hasGeneral && data.forum_mode) {
                await createGroupTopic(groupId, 'General', '💬', TOPIC_COLORS[0], true);
            }
            
            currentTopics.forEach(t => {
                unreadCounts.set(t.id, t.unread_count || 0);
            });
        }
        return data;
    } catch (error) {
        console.error('Error loading topics:', error);
        return { success: false, topics: [] };
    }
}

async function createGroupTopic(groupId, name, iconEmoji = '💬', iconColor = TOPIC_COLORS[0], isGeneral = false) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/create-group-topic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                token, group_id: groupId, name,
                icon_emoji: iconEmoji, icon_color: iconColor,
                is_general: isGeneral.toString()
            })
        });
        const data = await response.json();
        
        if (data.success && data.topic) {
            currentTopics.push(data.topic);
            if (!isGeneral) notifications.success('Тема создана');
        }
        return data;
    } catch (error) {
        console.error('Error creating topic:', error);
        return { success: false, error: error.message };
    }
}

async function updateGroupTopic(topicId, updates) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/update-group-topic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, topic_id: topicId, ...updates })
        });
        const data = await response.json();
        
        if (data.success) {
            const idx = currentTopics.findIndex(t => t.id === topicId);
            if (idx !== -1) currentTopics[idx] = { ...currentTopics[idx], ...updates };
            notifications.success('Тема обновлена');
        }
        return data;
    } catch (error) {
        console.error('Error updating topic:', error);
        return { success: false, error: error.message };
    }
}

async function deleteGroupTopic(topicId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/delete-group-topic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, topic_id: topicId })
        });
        const data = await response.json();
        
        if (data.success) {
            currentTopics = currentTopics.filter(t => t.id !== topicId);
            if (currentTopic && currentTopic.id === topicId) {
                currentTopic = null;
                currentTopicMessages = [];
            }
            notifications.success('Тема удалена');
        }
        return data;
    } catch (error) {
        console.error('Error deleting topic:', error);
        return { success: false, error: error.message };
    }
}

async function toggleTopicClosed(topicId, closed) {
    return updateGroupTopic(topicId, { is_closed: closed.toString() });
}

async function toggleTopicHidden(topicId, hidden) {
    return updateGroupTopic(topicId, { is_hidden: hidden.toString() });
}

async function loadTopicMessages(topicId, limit = MESSAGES_PER_PAGE, offsetId = null) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, messages: [] };
    
    console.log('[Topics] Loading messages for topic:', topicId);
    
    try {
        const body = { token, topic_id: topicId, limit: limit.toString() };
        if (offsetId) body.offset_id = offsetId;
        
        const response = await fetch('/api/get-topic-messages', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        const data = await response.json();
        
        console.log('[Topics] Loaded messages response:', { topicId, messagesCount: (data.messages || []).length, topic: data.topic });
        
        if (data.success) {
            currentTopic = data.topic;
            if (offsetId) {
                currentTopicMessages = [...currentTopicMessages, ...(data.messages || [])];
            } else {
                currentTopicMessages = data.messages || [];
            }
            hasMoreMessages = (data.messages || []).length === limit;
            unreadCounts.set(topicId, 0);
        }
        return data;
    } catch (error) {
        console.error('Error loading topic messages:', error);
        return { success: false, messages: [] };
    }
}

async function sendTopicMessage(topicId, content, options = {}) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/send-topic-message', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token, topic_id: topicId, content,
                message_type: options.message_type || 'text',
                file_path: options.file_path || '',
                file_name: options.file_name || '',
                file_size: options.file_size ? options.file_size.toString() : '0',
                reply_to_message_id: options.reply_to_message_id || ''
            })
        });
        const data = await response.json();
        
        if (data.success) {
            topicDraftMessages.delete(topicId);
            topicReplyToMessage = null;
        }
        return data;
    } catch (error) {
        console.error('Error sending topic message:', error);
        return { success: false, error: error.message };
    }
}

async function pinGroupTopic(topicId, order = 1) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    const pinnedCount = currentTopics.filter(t => t.pinned_order > 0).length;
    if (pinnedCount >= MAX_PINNED_TOPICS) {
        notifications.warning(`Максимум ${MAX_PINNED_TOPICS} закрепленных тем`);
        return { success: false, error: 'Max pinned topics reached' };
    }
    
    try {
        const response = await fetch('/api/pin-group-topic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, topic_id: topicId, order: order.toString() })
        });
        const data = await response.json();
        
        if (data.success) {
            const idx = currentTopics.findIndex(t => t.id === topicId);
            if (idx !== -1) currentTopics[idx].pinned_order = order;
            notifications.success('Тема закреплена');
        }
        return data;
    } catch (error) {
        console.error('Error pinning topic:', error);
        return { success: false, error: error.message };
    }
}

async function unpinGroupTopic(topicId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return { success: false, error: 'Not authenticated' };
    
    try {
        const response = await fetch('/api/unpin-group-topic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, topic_id: topicId })
        });
        const data = await response.json();
        
        if (data.success) {
            const idx = currentTopics.findIndex(t => t.id === topicId);
            if (idx !== -1) currentTopics[idx].pinned_order = 0;
            notifications.success('Тема откреплена');
        }
        return data;
    } catch (error) {
        console.error('Error unpinning topic:', error);
        return { success: false, error: error.message };
    }
}

async function setTopicNotifications(topicId, settings) {
    topicNotificationSettings.set(topicId, settings);
    return { success: true };
}

// =====================================================
// Topic Tabs (horizontal tabs above messages)
// =====================================================

function renderTopicTabs(groupId) {
    // Remove existing tabs
    const existingTabs = document.getElementById('topicTabsContainer');
    if (existingTabs) existingTabs.remove();
    
    // Find the chat header or messages container to insert before
    const chatContainer = document.querySelector('.chat-container') || document.querySelector('.chat-main');
    const chatMessages = document.getElementById('chatMessages');
    
    if (!chatMessages) return;
    
    const tabsContainer = document.createElement('div');
    tabsContainer.id = 'topicTabsContainer';
    tabsContainer.style.cssText = `
        display: flex;
        gap: 8px;
        padding: 8px 16px;
        background: var(--bg-secondary, #1e1e1e);
        border-bottom: 1px solid var(--border-color, #333);
        overflow-x: auto;
        flex-shrink: 0;
        scrollbar-width: none;
    `;
    
    // Add "New Topic" button for admins
    const userRole = window.currentGroupRole || 'member';
    
    // Sort topics
    const sortedTopics = [...currentTopics].sort((a, b) => {
        if (a.is_general) return -1;
        if (b.is_general) return 1;
        if (a.pinned_order && !b.pinned_order) return -1;
        if (!a.pinned_order && b.pinned_order) return 1;
        return new Date(b.last_message_at || 0) - new Date(a.last_message_at || 0);
    });
    
    sortedTopics.forEach(topic => {
        if (topic.is_hidden) return;
        
        const tab = document.createElement('button');
        tab.className = 'topic-tab' + (currentTopic && currentTopic.id === topic.id ? ' active' : '');
        tab.dataset.topicId = topic.id;
        tab.style.cssText = `
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 8px 14px;
            border-radius: 20px;
            border: none;
            background: ${currentTopic && currentTopic.id === topic.id ? 'var(--accent, #229ED9)' : 'var(--bg-tertiary, #2a2a2a)'};
            color: ${currentTopic && currentTopic.id === topic.id ? 'white' : 'var(--text-primary, #fff)'};
            font-size: 13px;
            font-weight: 500;
            cursor: pointer;
            white-space: nowrap;
            transition: all 0.2s ease;
            flex-shrink: 0;
        `;
        
        const iconColor = topic.icon_color || '#6fb1fc';
        tab.innerHTML = `
            <span style="font-size: 16px;">${topic.icon_emoji || '💬'}</span>
            <span>${escapeHtml(topic.name)}</span>
            ${topic.is_general ? '<span style="font-size: 10px; background: rgba(255,255,255,0.2); padding: 2px 6px; border-radius: 10px;">GENERAL</span>' : ''}
            ${topic.message_count > 0 ? `<span style="font-size: 11px; opacity: 0.7;">${topic.message_count}</span>` : ''}
        `;
        
        tab.onclick = () => openTopic(topic.id);
        
        // Hover effect
        tab.onmouseenter = () => {
            if (!tab.classList.contains('active')) {
                tab.style.background = 'var(--bg-hover, #3a3a3a)';
            }
        };
        tab.onmouseleave = () => {
            if (!tab.classList.contains('active')) {
                tab.style.background = 'var(--bg-tertiary, #2a2a2a)';
            }
        };
        
        tabsContainer.appendChild(tab);
    });
    
    // Add "+" button for new topic
    if (userRole === 'creator' || userRole === 'admin') {
        const addBtn = document.createElement('button');
        addBtn.className = 'topic-tab-add';
        addBtn.style.cssText = `
            display: flex;
            align-items: center;
            justify-content: center;
            width: 36px;
            height: 36px;
            border-radius: 50%;
            border: 2px dashed var(--border-color, #444);
            background: transparent;
            color: var(--text-muted, #888);
            font-size: 18px;
            cursor: pointer;
            flex-shrink: 0;
            transition: all 0.2s ease;
        `;
        addBtn.innerHTML = '+';
        addBtn.title = 'Новая тема';
        addBtn.onclick = () => showCreateTopicModal(groupId);
        addBtn.onmouseenter = () => {
            addBtn.style.borderColor = 'var(--accent, #229ED9)';
            addBtn.style.color = 'var(--accent, #229ED9)';
        };
        addBtn.onmouseleave = () => {
            addBtn.style.borderColor = 'var(--border-color, #444)';
            addBtn.style.color = 'var(--text-muted, #888)';
        };
        tabsContainer.appendChild(addBtn);
    }
    
    // Insert before chat messages
    chatMessages.parentNode.insertBefore(tabsContainer, chatMessages);
}

function updateActiveTab(topicId) {
    const tabs = document.querySelectorAll('.topic-tab');
    tabs.forEach(tab => {
        const isActive = tab.dataset.topicId === topicId;
        tab.classList.toggle('active', isActive);
        tab.style.background = isActive ? 'var(--accent, #229ED9)' : 'var(--bg-tertiary, #2a2a2a)';
        tab.style.color = isActive ? 'white' : 'var(--text-primary, #fff)';
    });
}

function hideTopicTabs() {
    const tabs = document.getElementById('topicTabsContainer');
    if (tabs) tabs.remove();
}

// =====================================================
// UI Rendering Functions  
// =====================================================

function renderForumPanel(container, groupId, userRole = 'member') {
    if (!container) return;
    
    currentGroupId = groupId;
    container.innerHTML = '';
    container.className = 'forum-panel';
    container.dataset.groupId = groupId;
    container.dataset.userRole = userRole;
    
    // Header
    const header = document.createElement('div');
    header.className = 'forum-header';
    header.innerHTML = `
        <div class="forum-header-left">
            <button class="forum-back-btn" onclick="closeForumPanel()">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none">
                    <path d="M15 18l-6-6 6-6" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                </svg>
            </button>
            <div class="forum-title">
                <h3>Темы</h3>
                <span class="forum-count">${currentTopics.length} тем</span>
            </div>
        </div>
        <div class="forum-header-right">
            <button class="forum-search-btn" onclick="toggleTopicSearch()" title="Поиск">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
                    <circle cx="11" cy="11" r="7" stroke="currentColor" stroke-width="2"/>
                    <path d="M21 21l-4-4" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                </svg>
            </button>
            <button class="forum-view-btn" onclick="toggleViewMode('${groupId}')" title="Режим просмотра">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
                    <path d="M4 6h16M4 12h16M4 18h16" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                </svg>
            </button>
            ${(userRole === 'creator' || userRole === 'admin') ? `
                <button class="forum-settings-btn" onclick="showForumSettings('${groupId}')" title="Настройки">
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
                        <circle cx="12" cy="12" r="3" stroke="currentColor" stroke-width="2"/>
                        <path d="M12 1v4M12 19v4M4.22 4.22l2.83 2.83M16.95 16.95l2.83 2.83M1 12h4M19 12h4M4.22 19.78l2.83-2.83M16.95 7.05l2.83-2.83" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
                    </svg>
                </button>
            ` : ''}
        </div>
    `;
    container.appendChild(header);
    
    // Search bar
    const searchBar = document.createElement('div');
    searchBar.className = 'forum-search-bar hidden';
    searchBar.id = 'forumSearchBar';
    searchBar.innerHTML = `
        <input type="text" placeholder="Поиск тем..." id="topicSearchInput" oninput="handleTopicSearch('${groupId}', this.value)">
        <button class="search-clear-btn" onclick="clearTopicSearch('${groupId}')">×</button>
    `;
    container.appendChild(searchBar);
    
    // Topics list
    renderTopicsListContent(container, groupId, userRole);
    
    // FAB for creating topics
    if (userRole === 'creator' || userRole === 'admin') {
        const fab = document.createElement('button');
        fab.className = 'forum-fab';
        fab.onclick = () => showCreateTopicModal(groupId);
        fab.innerHTML = `
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none">
                <path d="M12 5v14M5 12h14" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
            </svg>
        `;
        container.appendChild(fab);
    }
}

function renderTopicsListContent(container, groupId, userRole) {
    const list = document.createElement('div');
    list.className = 'topics-list';
    list.id = 'topicsList';
    
    if (currentTopics.length === 0) {
        list.innerHTML = `
            <div class="topics-empty">
                <div class="empty-icon">💬</div>
                <h4>Нет тем</h4>
                <p>Создайте первую тему для обсуждения</p>
            </div>
        `;
    } else {
        // Sort topics: General first, then pinned, then by activity
        const generalTopic = currentTopics.find(t => t.is_general && !t.is_hidden);
        const pinnedTopics = currentTopics.filter(t => t.pinned_order > 0 && !t.is_general)
            .sort((a, b) => a.pinned_order - b.pinned_order);
        const otherTopics = currentTopics.filter(t => !t.pinned_order && !t.is_general)
            .sort((a, b) => new Date(b.last_message_at) - new Date(a.last_message_at));
        
        // Pinned section
        if (pinnedTopics.length > 0) {
            const section = document.createElement('div');
            section.className = 'topics-section';
            section.innerHTML = `<div class="section-label">📌 Закрепленные</div>`;
            pinnedTopics.forEach(topic => {
                section.appendChild(createTopicElement(topic, userRole, groupId));
            });
            list.appendChild(section);
        }
        
        // General topic
        if (generalTopic) {
            list.appendChild(createTopicElement(generalTopic, userRole, groupId));
        }
        
        // Other topics
        if (otherTopics.length > 0) {
            const section = document.createElement('div');
            section.className = 'topics-section';
            if (pinnedTopics.length > 0 || generalTopic) {
                section.innerHTML = `<div class="section-label">Темы</div>`;
            }
            otherTopics.forEach(topic => {
                section.appendChild(createTopicElement(topic, userRole, groupId));
            });
            list.appendChild(section);
        }
    }
    
    container.appendChild(list);
}

function createTopicElement(topic, userRole, groupId) {
    const el = document.createElement('div');
    el.className = 'topic-item';
    if (currentTopic && currentTopic.id === topic.id) el.classList.add('active');
    if (topic.is_closed) el.classList.add('closed');
    if (topic.is_general) el.classList.add('general');
    el.dataset.topicId = topic.id;
    
    const unread = unreadCounts.get(topic.id) || 0;
    const draft = topicDraftMessages.get(topic.id);
    
    el.innerHTML = `
        <div class="topic-icon-wrapper">
            <div class="topic-icon" style="background: ${topic.icon_color || TOPIC_COLORS[0]}">
                ${topic.icon_emoji || '💬'}
            </div>
            ${topic.is_closed ? '<div class="topic-lock">🔒</div>' : ''}
        </div>
        <div class="topic-content">
            <div class="topic-header">
                <span class="topic-name">${escapeHtml(topic.name)}</span>
                <span class="topic-time">${formatTopicTime(topic.last_message_at)}</span>
            </div>
            <div class="topic-preview">
                ${draft ? 
                    `<span class="topic-draft">Черновик: ${escapeHtml(draft.substring(0, 30))}...</span>` :
                    (topic.last_message ? 
                        `<span class="topic-sender">${escapeHtml(topic.last_message_sender || '')}:</span> ${escapeHtml((topic.last_message || '').substring(0, 35))}` : 
                        '<span class="topic-empty-msg">Нет сообщений</span>'
                    )
                }
            </div>
        </div>
        <div class="topic-meta">
            ${unread > 0 ? `<span class="topic-unread">${unread > 99 ? '99+' : unread}</span>` : ''}
            ${topic.pinned_order > 0 ? '<span class="topic-pin">📌</span>' : ''}
        </div>
    `;
    
    el.onclick = () => openTopic(topic.id);
    
    // Context menu
    el.oncontextmenu = (e) => {
        e.preventDefault();
        showTopicContextMenu(topic.id, e, userRole);
    };
    
    return el;
}

function renderTopicChatView(topic, userRole) {
    // Update chat header
    const headerName = document.getElementById('chatHeaderName');
    const headerAvatar = document.getElementById('chatHeaderAvatar');
    const headerStatus = document.querySelector('.chat-header-status');
    
    if (headerName) {
        headerName.innerHTML = `<span class="chat-name-text">${escapeHtml(topic.name)}</span>`;
    }
    if (headerAvatar) {
        headerAvatar.style.background = topic.icon_color || '#6fb1fc';
        headerAvatar.textContent = topic.icon_emoji || '💬';
    }
    if (headerStatus) {
        headerStatus.textContent = `${topic.message_count || 0} сообщений`;
    }
    
    // Render messages in main chat area
    const chatMessages = document.getElementById('chatMessages');
    if (!chatMessages) return;
    
    // Store current topic for message sending
    window.currentActiveTopic = topic;
    
    // Render topic messages
    renderTopicMessages();
    
    // Update input area for topic
    const inputArea = document.querySelector('.chat-input-area');
    if (inputArea && !topic.is_closed) {
        // Update send button to use topic sending
        const sendBtn = document.getElementById('sendBtn');
        if (sendBtn) {
            sendBtn.onclick = () => sendCurrentTopicMessage(topic.id);
        }
        // Update input keydown handler
        const messageInput = document.getElementById('messageInput');
        if (messageInput) {
            messageInput.onkeydown = (e) => {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    sendCurrentTopicMessage(topic.id);
                }
            };
        }
    }
    
    // Show back button in forum panel to return to topic list
    const forumPanel = document.querySelector('.forum-panel');
    if (forumPanel) {
        const topicsHeader = forumPanel.querySelector('.topics-header');
        if (topicsHeader && !topicsHeader.querySelector('.topic-back-btn')) {
            const backBtn = document.createElement('button');
            backBtn.className = 'topic-back-btn';
            backBtn.innerHTML = `<svg width="20" height="20" viewBox="0 0 24 24" fill="none"><path d="M15 18l-6-6 6-6" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`;
            backBtn.onclick = closeTopicChat;
            topicsHeader.insertBefore(backBtn, topicsHeader.firstChild);
        }
    }
}

function renderTopicMessages() {
    // Use main chat messages container
    const container = document.getElementById('chatMessages');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (currentTopicMessages.length === 0) {
        container.innerHTML = `
            <div class="no-messages" style="display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100%; color: var(--text-muted);">
                <div style="font-size: 48px; margin-bottom: 16px;">💬</div>
                <p style="font-size: 16px; margin: 0;">Нет сообщений</p>
                <p style="font-size: 14px; margin-top: 8px; opacity: 0.7;">Начните обсуждение!</p>
            </div>
        `;
        return;
    }
    
    // Group by date
    const byDate = {};
    currentTopicMessages.forEach(msg => {
        const date = formatMessageDate(msg.created_at);
        if (!byDate[date]) byDate[date] = [];
        byDate[date].push(msg);
    });
    
    Object.entries(byDate).forEach(([date, messages]) => {
        // Date separator
        const sep = document.createElement('div');
        sep.className = 'message-date-sep';
        sep.style.cssText = 'text-align: center; padding: 10px; color: var(--text-muted); font-size: 12px;';
        sep.innerHTML = `<span style="background: var(--bg-secondary); padding: 4px 12px; border-radius: 12px;">${date}</span>`;
        container.appendChild(sep);
        
        // Messages
        messages.forEach(msg => {
            container.appendChild(createTopicMessageElement(msg));
        });
    });
    
    // Scroll to bottom
    container.scrollTop = container.scrollHeight;
}

function createTopicMessageElement(msg) {
    const currentUserId = localStorage.getItem('xipher_user_id');
    const isOwn = msg.sender_id === currentUserId;
    
    const el = document.createElement('div');
    el.className = 'message' + (isOwn ? ' own' : '');
    el.dataset.messageId = msg.id;
    el.style.cssText = `display: flex; gap: 8px; padding: 4px 16px; ${isOwn ? 'flex-direction: row-reverse;' : ''}`;
    
    const time = formatMessageTime(msg.created_at);
    
    el.innerHTML = `
        ${!isOwn ? `
            <div class="message-avatar" style="width: 36px; height: 36px; border-radius: 50%; background: ${getNameColor(msg.sender_id)}; display: flex; align-items: center; justify-content: center; color: white; font-weight: 600; flex-shrink: 0;">
                ${(msg.sender_username || 'U').charAt(0).toUpperCase()}
            </div>
        ` : ''}
        <div class="message-bubble" style="max-width: 70%; padding: 8px 12px; border-radius: 12px; background: ${isOwn ? 'var(--accent, #229ED9)' : 'var(--bg-secondary, #2a2a2a)'}; color: ${isOwn ? 'white' : 'var(--text-primary, #fff)'};">
            ${!isOwn ? `<div class="message-sender" style="font-size: 13px; font-weight: 600; color: ${getNameColor(msg.sender_id)}; margin-bottom: 2px;">${escapeHtml(msg.sender_username || 'Unknown')}</div>` : ''}
            <div class="message-content" style="word-break: break-word;">${escapeHtml(msg.content)}</div>
            <div class="message-time" style="font-size: 11px; opacity: 0.7; text-align: right; margin-top: 4px;">${time}</div>
        </div>
    `;
    
    return el;
}

function renderMessageContent(msg) {
    switch (msg.message_type) {
        case 'image':
            return `<img class="message-image" src="${msg.file_path}" alt="" onclick="openImageViewer('${msg.file_path}')">`;
        case 'video':
            return `<video class="message-video" src="${msg.file_path}" controls></video>`;
        case 'file':
            return `
                <div class="message-file">
                    <span class="file-icon">📎</span>
                    <div class="file-info">
                        <span class="file-name">${escapeHtml(msg.file_name)}</span>
                        <span class="file-size">${formatFileSize(msg.file_size)}</span>
                    </div>
                </div>
            `;
        default:
            return `<div class="message-text">${formatMessageText(msg.content)}</div>`;
    }
}

// =====================================================
// Modals
// =====================================================

function showCreateTopicModal(groupId) {
    closeAllModals();
    
    const modal = document.createElement('div');
    modal.className = 'modal-overlay topic-modal';
    modal.id = 'createTopicModal';
    
    modal.innerHTML = `
        <div class="modal-content create-topic-modal">
            <div class="modal-header">
                <h3>Новая тема</h3>
                <button class="modal-close" onclick="closeCreateTopicModal()">×</button>
            </div>
            <div class="modal-body">
                <div class="topic-preview-row">
                    <div class="topic-preview-icon" id="previewIcon" style="background: ${TOPIC_COLORS[0]}">💬</div>
                    <input type="text" class="topic-name-input" id="topicNameInput" placeholder="Название темы" maxlength="128" autofocus>
                </div>
                
                <div class="topic-customization">
                    <div class="customization-section">
                        <label>Иконка</label>
                        <div class="emoji-grid" id="emojiGrid">
                            ${TOPIC_EMOJIS.map((e, i) => `
                                <button class="emoji-btn${i === 0 ? ' selected' : ''}" data-emoji="${e}" onclick="selectTopicEmoji('${e}')">${e}</button>
                            `).join('')}
                        </div>
                    </div>
                    <div class="customization-section">
                        <label>Цвет</label>
                        <div class="color-grid" id="colorGrid">
                            ${TOPIC_COLORS.map((c, i) => `
                                <button class="color-btn${i === 0 ? ' selected' : ''}" data-color="${c}" style="background: ${c}" onclick="selectTopicColor('${c}')"></button>
                            `).join('')}
                        </div>
                    </div>
                </div>
            </div>
            <div class="modal-footer">
                <button class="btn-secondary" onclick="closeCreateTopicModal()">Отмена</button>
                <button class="btn-primary" onclick="submitCreateTopic('${groupId}')">Создать</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    modal.onclick = (e) => { if (e.target === modal) closeCreateTopicModal(); };
    document.getElementById('topicNameInput').focus();
}

function selectTopicEmoji(emoji) {
    document.querySelectorAll('#emojiGrid .emoji-btn').forEach(b => b.classList.remove('selected'));
    { const _el = document.querySelector(`#emojiGrid .emoji-btn[data-emoji="${emoji}"]`); if (_el) _el.classList.add('selected'); };
    document.getElementById('previewIcon').textContent = emoji;
}

function selectTopicColor(color) {
    document.querySelectorAll('#colorGrid .color-btn').forEach(b => b.classList.remove('selected'));
    { const _el = document.querySelector(`#colorGrid .color-btn[data-color="${color}"]`); if (_el) _el.classList.add('selected'); };
    document.getElementById('previewIcon').style.background = color;
}

async function submitCreateTopic(groupId) {
    const nameInput = document.getElementById('topicNameInput');
    const name = (nameInput && nameInput.value) ? nameInput.value.trim() : '';
    if (!name) {
        notifications.error('Введите название темы');
        return;
    }
    
    const emoji = (function() { const _el = document.querySelector('#emojiGrid .emoji-btn.selected'); return _el ? _el.dataset.emoji : undefined; })() || '💬';
    const color = (function() { const _el = document.querySelector('#colorGrid .color-btn.selected'); return _el ? _el.dataset.color : undefined; })() || TOPIC_COLORS[0];
    
    const result = await createGroupTopic(groupId, name, emoji, color);
    if (result.success) {
        closeCreateTopicModal();
        await loadGroupTopics(groupId);
        refreshCurrentView();
    }
}

function closeCreateTopicModal() {
    { const _el = document.getElementById('createTopicModal'); if (_el) _el.remove(); };
}

function showEditTopicModal(topicId) {
    const topic = currentTopics.find(t => t.id === topicId);
    if (!topic) return;
    
    closeAllModals();
    
    const modal = document.createElement('div');
    modal.className = 'modal-overlay topic-modal';
    modal.id = 'editTopicModal';
    
    modal.innerHTML = `
        <div class="modal-content edit-topic-modal">
            <div class="modal-header">
                <h3>Редактировать тему</h3>
                <button class="modal-close" onclick="closeEditTopicModal()">×</button>
            </div>
            <div class="modal-body">
                <div class="topic-preview-row">
                    <div class="topic-preview-icon" id="editPreviewIcon" style="background: ${topic.icon_color}">${topic.icon_emoji || '💬'}</div>
                    <input type="text" class="topic-name-input" id="editTopicNameInput" value="${escapeHtml(topic.name)}" maxlength="128">
                </div>
                
                <div class="topic-customization">
                    <div class="customization-section">
                        <label>Иконка</label>
                        <div class="emoji-grid" id="editEmojiGrid">
                            ${TOPIC_EMOJIS.map(e => `
                                <button class="emoji-btn${e === topic.icon_emoji ? ' selected' : ''}" data-emoji="${e}" onclick="selectEditEmoji('${e}')">${e}</button>
                            `).join('')}
                        </div>
                    </div>
                    <div class="customization-section">
                        <label>Цвет</label>
                        <div class="color-grid" id="editColorGrid">
                            ${TOPIC_COLORS.map(c => `
                                <button class="color-btn${c === topic.icon_color ? ' selected' : ''}" data-color="${c}" style="background: ${c}" onclick="selectEditColor('${c}')"></button>
                            `).join('')}
                        </div>
                    </div>
                </div>
                
                <div class="topic-options">
                    <label class="option-row">
                        <span>Закрыть тему</span>
                        <input type="checkbox" id="topicClosedCheck" ${topic.is_closed ? 'checked' : ''}>
                        <span class="toggle-switch"></span>
                    </label>
                </div>
            </div>
            <div class="modal-footer">
                <button class="btn-danger" onclick="confirmDeleteTopic('${topicId}')">Удалить</button>
                <div class="footer-spacer"></div>
                <button class="btn-secondary" onclick="closeEditTopicModal()">Отмена</button>
                <button class="btn-primary" onclick="submitEditTopic('${topicId}')">Сохранить</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    modal.onclick = (e) => { if (e.target === modal) closeEditTopicModal(); };
}

function selectEditEmoji(emoji) {
    document.querySelectorAll('#editEmojiGrid .emoji-btn').forEach(b => b.classList.remove('selected'));
    { const _el = document.querySelector(`#editEmojiGrid .emoji-btn[data-emoji="${emoji}"]`); if (_el) _el.classList.add('selected'); };
    document.getElementById('editPreviewIcon').textContent = emoji;
}

function selectEditColor(color) {
    document.querySelectorAll('#editColorGrid .color-btn').forEach(b => b.classList.remove('selected'));
    { const _el = document.querySelector(`#editColorGrid .color-btn[data-color="${color}"]`); if (_el) _el.classList.add('selected'); };
    document.getElementById('editPreviewIcon').style.background = color;
}

async function submitEditTopic(topicId) {
    const nameInput = document.getElementById('editTopicNameInput');
    const name = (nameInput && nameInput.value) ? nameInput.value.trim() : '';
    if (!name) {
        notifications.error('Введите название темы');
        return;
    }
    
    const emoji = (function() { const _el = document.querySelector('#editEmojiGrid .emoji-btn.selected'); return _el ? _el.dataset.emoji : undefined; })();
    const color = (function() { const _el = document.querySelector('#editColorGrid .color-btn.selected'); return _el ? _el.dataset.color : undefined; })();
    const isClosed = (function() { const _el = document.getElementById('topicClosedCheck'); return _el ? _el.checked : false; })();
    
    await updateGroupTopic(topicId, { name, icon_emoji: emoji, icon_color: color, is_closed: (isClosed ? isClosed.toString() : 'false') });
    closeEditTopicModal();
    refreshCurrentView();
}

function closeEditTopicModal() {
    { const _el = document.getElementById('editTopicModal'); if (_el) _el.remove(); };
}

function confirmDeleteTopic(topicId) {
    const topic = currentTopics.find(t => t.id === topicId);
    if (!topic || topic.is_general) {
        notifications.error('Нельзя удалить тему General');
        return;
    }
    
    if (confirm(`Удалить тему "${topic.name}"?`)) {
        deleteGroupTopic(topicId).then(() => {
            closeEditTopicModal();
            closeTopicChat();
            refreshCurrentView();
        });
    }
}

function showTopicContextMenu(topicId, event, userRole) {
    closeAllContextMenus();
    
    const topic = currentTopics.find(t => t.id === topicId);
    if (!topic) return;
    
    const isAdmin = userRole === 'creator' || userRole === 'admin';
    
    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.id = 'topicContextMenu';
    
    let html = `<button class="context-item" onclick="openTopic('${topicId}'); closeAllContextMenus();">💬 Открыть</button>`;
    
    if (isAdmin) {
        if (topic.pinned_order > 0) {
            html += `<button class="context-item" onclick="unpinGroupTopic('${topicId}').then(refreshCurrentView); closeAllContextMenus();">📌 Открепить</button>`;
        } else {
            html += `<button class="context-item" onclick="pinGroupTopic('${topicId}').then(refreshCurrentView); closeAllContextMenus();">📌 Закрепить</button>`;
        }
        
        html += topic.is_closed ?
            `<button class="context-item" onclick="toggleTopicClosed('${topicId}', false).then(refreshCurrentView); closeAllContextMenus();">🔓 Открыть тему</button>` :
            `<button class="context-item" onclick="toggleTopicClosed('${topicId}', true).then(refreshCurrentView); closeAllContextMenus();">🔒 Закрыть тему</button>`;
        
        if (!topic.is_general) {
            html += `<button class="context-item" onclick="showEditTopicModal('${topicId}'); closeAllContextMenus();">✏️ Редактировать</button>`;
            html += `<div class="context-divider"></div>`;
            html += `<button class="context-item danger" onclick="confirmDeleteTopic('${topicId}'); closeAllContextMenus();">🗑️ Удалить</button>`;
        }
    }
    
    menu.innerHTML = html;
    
    const x = event.clientX || 0;
    const y = event.clientY || 0;
    menu.style.left = `${Math.min(x, window.innerWidth - 200)}px`;
    menu.style.top = `${Math.min(y, window.innerHeight - 250)}px`;
    
    document.body.appendChild(menu);
    setTimeout(() => document.addEventListener('click', closeAllContextMenus, { once: true }), 0);
}

function showMessageContextMenu(msg, event) {
    closeAllContextMenus();
    
    const isOwn = msg.sender_id === localStorage.getItem('xipher_user_id');
    
    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.id = 'messageContextMenu';
    
    menu.innerHTML = `
        <button class="context-item" onclick="setReplyTo(${JSON.stringify(msg).replace(/"/g, '&quot;')}); closeAllContextMenus();">↩️ Ответить</button>
        <button class="context-item" onclick="copyText('${escapeHtml(msg.content)}'); closeAllContextMenus();">📋 Копировать</button>
        ${isOwn ? `
            <div class="context-divider"></div>
            <button class="context-item danger" onclick="deleteMessage('${msg.id}'); closeAllContextMenus();">🗑️ Удалить</button>
        ` : ''}
    `;
    
    menu.style.left = `${Math.min(event.clientX, window.innerWidth - 180)}px`;
    menu.style.top = `${Math.min(event.clientY, window.innerHeight - 150)}px`;
    
    document.body.appendChild(menu);
    setTimeout(() => document.addEventListener('click', closeAllContextMenus, { once: true }), 0);
}

function showForumSettings(groupId) {
    closeAllModals();
    
    const modal = document.createElement('div');
    modal.className = 'modal-overlay topic-modal';
    modal.id = 'forumSettingsModal';
    
    modal.innerHTML = `
        <div class="modal-content forum-settings-modal">
            <div class="modal-header">
                <h3>Настройки форума</h3>
                <button class="modal-close" onclick="closeForumSettingsModal()">×</button>
            </div>
            <div class="modal-body">
                <div class="settings-section">
                    <label class="option-row">
                        <span>Режим вкладок</span>
                        <input type="checkbox" ${topicUIMode === 'tabs' ? 'checked' : ''} onchange="setUIMode(this.checked ? 'tabs' : 'list')">
                        <span class="toggle-switch"></span>
                    </label>
                    <label class="option-row">
                        <span>Показывать как сообщения</span>
                        <input type="checkbox" ${viewAsMessages ? 'checked' : ''} onchange="toggleViewAsMessages('${groupId}', this.checked)">
                        <span class="toggle-switch"></span>
                    </label>
                </div>
                <div class="settings-section">
                    <button class="btn-danger full-width" onclick="confirmDisableForumMode('${groupId}')">
                        Отключить режим форума
                    </button>
                    <p class="hint">Все темы будут удалены</p>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    modal.onclick = (e) => { if (e.target === modal) closeForumSettingsModal(); };
}

function closeForumSettingsModal() {
    { const _el = document.getElementById('forumSettingsModal'); if (_el) _el.remove(); };
}

// =====================================================
// Event Handlers
// =====================================================

async function openTopic(topicId) {
    console.log('[Topics] Opening topic:', topicId);
    
    const result = await loadTopicMessages(topicId);
    if (!result.success) {
        notifications.error('Не удалось загрузить тему');
        return;
    }
    
    currentTopic = result.topic;
    window.currentActiveTopic = result.topic;
    
    // Update active tab
    updateActiveTab(topicId);
    
    // Update chat header with topic info
    const headerName = document.getElementById('chatHeaderName');
    const headerAvatar = document.getElementById('chatHeaderAvatar');
    const headerStatus = document.querySelector('.chat-header-status');
    
    if (headerName) {
        headerName.innerHTML = `<span class="chat-name-text">${escapeHtml(currentTopic.name)}</span>`;
    }
    if (headerAvatar) {
        headerAvatar.style.background = currentTopic.icon_color || '#6fb1fc';
        headerAvatar.textContent = currentTopic.icon_emoji || '💬';
    }
    if (headerStatus) {
        headerStatus.textContent = `${currentTopic.message_count || 0} сообщений`;
    }
    
    // Render messages
    renderTopicMessages();
    
    // Setup input for topic
    setupTopicInput(topicId);
}

function setupTopicInput(topicId) {
    const sendBtn = document.getElementById('sendBtn');
    if (sendBtn) {
        sendBtn.onclick = () => sendCurrentTopicMessage(topicId);
    }
    const messageInput = document.getElementById('messageInput');
    if (messageInput) {
        messageInput.placeholder = 'Сообщение в тему...';
        messageInput.onkeydown = (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendCurrentTopicMessage(topicId);
            }
        };
    }
}

function closeTopicChat() {
    currentTopic = null;
    window.currentActiveTopic = null;
    currentTopicMessages = [];
    topicReplyToMessage = null;
    
    // Hide topic tabs
    hideTopicTabs();
    
    // Restore original input handlers
    const sendBtn = document.getElementById('sendBtn');
    if (sendBtn) {
        sendBtn.onclick = null; // Will use default
    }
    const messageInput = document.getElementById('messageInput');
    if (messageInput) {
        messageInput.placeholder = 'Сообщение...';
        messageInput.onkeydown = null; // Will use default
        messageInput.value = '';
    }
    
    refreshCurrentView();
}

async function sendCurrentTopicMessage(topicId) {
    // Use main message input
    const input = document.getElementById('messageInput');
    const content = ((input && input.value) ? input.value.trim() : '');
    if (!content) return;
    
    const options = {};
    if (topicReplyToMessage) options.reply_to_message_id = topicReplyToMessage.id;
    
    const result = await sendTopicMessage(topicId, content, options);
    if (result.success) {
        input.value = '';
        input.style.height = 'auto';
        topicReplyToMessage = null;
        removeReplyPreview();
        
        await loadTopicMessages(topicId);
        renderTopicMessages();
    } else {
        notifications.error(result.error || 'Ошибка отправки');
    }
}

function setReplyTo(msg) {
    topicReplyToMessage = msg;
    showReplyPreview(msg);
    { const _el = document.getElementById('topicInput'); if (_el) _el.focus(); };
}

function showReplyPreview(msg) {
    removeReplyPreview();
    
    const inputArea = document.getElementById('topicInputArea');
    if (!inputArea) return;
    
    const preview = document.createElement('div');
    preview.className = 'reply-preview';
    preview.id = 'replyPreview';
    preview.innerHTML = `
        <div class="reply-bar"></div>
        <div class="reply-content">
            <span class="reply-name">${escapeHtml(msg.sender_name)}</span>
            <span class="reply-text">${escapeHtml((msg.content ? msg.content.substring(0, 50) : '') || '')}</span>
        </div>
        <button class="reply-close" onclick="cancelReply()">×</button>
    `;
    inputArea.insertBefore(preview, inputArea.firstChild);
}

function removeReplyPreview() {
    { const _el = document.getElementById('replyPreview'); if (_el) _el.remove(); };
}

function cancelReply() {
    topicReplyToMessage = null;
    removeReplyPreview();
}

function saveDraft(topicId, text) {
    if ((text ? text.trim() : '')) {
        topicDraftMessages.set(topicId, text);
    } else {
        topicDraftMessages.delete(topicId);
    }
}

function handleInputKeydown(event, topicId) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        sendCurrentTopicMessage(topicId);
    }
}

function toggleTopicSearch() {
    const bar = document.getElementById('forumSearchBar');
    (bar && bar.classList).toggle('hidden');
    if (!(bar && bar.classList).contains('hidden')) {
        { const _el = document.getElementById('topicSearchInput'); if (_el) _el.focus(); };
    }
}

async function handleTopicSearch(groupId, query) {
    searchQuery = query;
    await loadGroupTopics(groupId, query);
    refreshCurrentView();
}

function clearTopicSearch(groupId) {
    searchQuery = '';
    document.getElementById('topicSearchInput').value = '';
    handleTopicSearch(groupId, '');
}

function toggleViewMode(groupId) {
    viewAsMessages = !viewAsMessages;
    refreshCurrentView();
}

function setUIMode(mode) {
    topicUIMode = mode;
    refreshCurrentView();
}

async function confirmDisableForumMode(groupId) {
    if (confirm('Отключить режим форума? Все темы будут удалены.')) {
        await setGroupForumMode(groupId, false);
        closeForumSettingsModal();
        if (typeof selectGroup === 'function') selectGroup(groupId);
    }
}

function refreshCurrentView() {
    const panel = document.querySelector('.forum-panel');
    if (panel && currentGroupId) {
        renderForumPanel(panel, currentGroupId, panel.dataset.userRole || 'member');
    }
}

function closeForumPanel() {
    currentTopic = null;
    currentTopicMessages = [];
    
    const chatArea = document.querySelector('.chat-main, #chatArea');
    if ((chatArea && chatArea.dataset).originalContent) {
        chatArea.innerHTML = chatArea.dataset.originalContent;
        delete chatArea.dataset.originalContent;
    }
}

// =====================================================
// Utility Functions
// =====================================================

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function formatTopicTime(dateStr) {
    if (!dateStr) return '';
    const date = new Date(dateStr);
    const now = new Date();
    const diff = now - date;
    
    if (diff < 60000) return 'сейчас';
    if (diff < 3600000) return Math.floor(diff / 60000) + ' мин';
    if (diff < 86400000) return Math.floor(diff / 3600000) + ' ч';
    if (diff < 604800000) return Math.floor(diff / 86400000) + ' д';
    return date.toLocaleDateString('ru-RU', { day: 'numeric', month: 'short' });
}

function formatMessageTime(dateStr) {
    if (!dateStr) return '';
    return new Date(dateStr).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
}

function formatMessageDate(dateStr) {
    if (!dateStr) return '';
    const date = new Date(dateStr);
    const now = new Date();
    const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    const yesterday = new Date(today - 86400000);
    
    if (date >= today) return 'Сегодня';
    if (date >= yesterday) return 'Вчера';
    return date.toLocaleDateString('ru-RU', { day: 'numeric', month: 'long' });
}

function formatFileSize(bytes) {
    if (!bytes) return '0 Б';
    const units = ['Б', 'КБ', 'МБ', 'ГБ'];
    let i = 0;
    while (bytes >= 1024 && i < units.length - 1) { bytes /= 1024; i++; }
    return bytes.toFixed(1) + ' ' + units[i];
}

function formatMessageText(text) {
    if (!text) return '';
    let f = escapeHtml(text);
    f = f.replace(/(https?:\/\/[^\s]+)/g, '<a href="$1" target="_blank">$1</a>');
    f = f.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
    f = f.replace(/__(.+?)__/g, '<em>$1</em>');
    f = f.replace(/`(.+?)`/g, '<code>$1</code>');
    f = f.replace(/\n/g, '<br>');
    return f;
}

function getNameColor(id) {
    const colors = ['#e17076', '#7bc862', '#e5ca77', '#65aadd', '#a695e7', '#ee7aae', '#6ec9cb', '#faa774'];
    const hash = (id || '').split('').reduce((a, c) => a + c.charCodeAt(0), 0);
    return colors[hash % colors.length];
}

function autoResizeInput(textarea) {
    textarea.style.height = 'auto';
    textarea.style.height = Math.min(textarea.scrollHeight, 150) + 'px';
}

function closeAllModals() {
    document.querySelectorAll('.modal-overlay').forEach(m => m.remove());
}

function closeAllContextMenus() {
    document.querySelectorAll('.context-menu').forEach(m => m.remove());
}

function copyText(text) {
    navigator.clipboard.writeText(text).then(() => notifications.success('Скопировано'));
}

function scrollToMessage(messageId) {
    const el = document.querySelector(`[data-message-id="${messageId}"]`);
    if (el) {
        el.scrollIntoView({ behavior: 'smooth', block: 'center' });
        el.classList.add('highlighted');
        setTimeout(() => el.classList.remove('highlighted'), 2000);
    }
}

// =====================================================
// WebSocket Handler
// =====================================================

function handleTopicWebSocketMessage(data) {
    switch (data.type) {
        case 'topic_message':
            if (currentTopic && currentTopic.id === data.topic_id) {
                loadTopicMessages(currentTopic.id).then(renderTopicMessages);
            } else {
                unreadCounts.set(data.topic_id, (unreadCounts.get(data.topic_id) || 0) + 1);
            }
            break;
        case 'topic_created':
        case 'topic_updated':
        case 'topic_deleted':
            if (currentGroupId) loadGroupTopics(currentGroupId).then(refreshCurrentView);
            break;
    }
}

// =====================================================
// Legacy Functions
// =====================================================

function renderTopicsList(container, groupId, userRole) {
    renderForumPanel(container, groupId, userRole);
}

function renderForumModeToggle(container, groupId, enabled, userRole) {
    if (userRole !== 'creator' && userRole !== 'admin') return;
    const toggle = document.createElement('div');
    toggle.className = 'forum-mode-toggle';
    toggle.innerHTML = `
        <label class="toggle-label">
            <span>Режим форума</span>
            <input type="checkbox" ${enabled ? 'checked' : ''} onchange="toggleForumMode('${groupId}', this.checked)">
            <span class="toggle-switch"></span>
        </label>
    `;
    container.appendChild(toggle);
}

async function toggleForumMode(groupId, enabled) {
    await setGroupForumMode(groupId, enabled);
    if (typeof selectGroup === 'function') selectGroup(groupId);
}

// =====================================================
// Export
// =====================================================

window.topicsModule = {
    get currentTopics() { return currentTopics; },
    get currentTopic() { return currentTopic; },
    get forumModeGroups() { return forumModeGroups; },
    
    setGroupForumMode, loadGroupTopics, createGroupTopic, updateGroupTopic,
    deleteGroupTopic, toggleTopicClosed, toggleTopicHidden, loadTopicMessages,
    sendTopicMessage, pinGroupTopic, unpinGroupTopic, setTopicNotifications,
    
    renderForumPanel, renderTopicsList, showCreateTopicModal, showEditTopicModal,
    openTopic, closeTopicChat, renderForumModeToggle,
    renderTopicTabs, updateActiveTab, hideTopicTabs,
    
    handleTopicWebSocketMessage,
    
    TOPIC_COLORS, TOPIC_EMOJIS, MAX_PINNED_TOPICS
};

// Global functions
window.showCreateTopicModal = showCreateTopicModal;
window.closeCreateTopicModal = closeCreateTopicModal;
window.submitCreateTopic = submitCreateTopic;
window.selectTopicEmoji = selectTopicEmoji;
window.selectTopicColor = selectTopicColor;
window.showEditTopicModal = showEditTopicModal;
window.closeEditTopicModal = closeEditTopicModal;
window.submitEditTopic = submitEditTopic;
window.selectEditEmoji = selectEditEmoji;
window.selectEditColor = selectEditColor;
window.confirmDeleteTopic = confirmDeleteTopic;
window.showTopicContextMenu = showTopicContextMenu;
window.openTopic = openTopic;
window.closeTopicChat = closeTopicChat;
window.sendCurrentTopicMessage = sendCurrentTopicMessage;
window.setReplyTo = setReplyTo;
window.cancelReply = cancelReply;
window.toggleForumMode = toggleForumMode;
window.toggleTopicSearch = toggleTopicSearch;
window.handleTopicSearch = handleTopicSearch;
window.clearTopicSearch = clearTopicSearch;
window.toggleViewMode = toggleViewMode;
window.showForumSettings = showForumSettings;
window.closeForumSettingsModal = closeForumSettingsModal;
window.pinGroupTopic = pinGroupTopic;
window.unpinGroupTopic = unpinGroupTopic;
window.toggleTopicClosed = toggleTopicClosed;
window.toggleTopicHidden = toggleTopicHidden;
window.closeForumPanel = closeForumPanel;
window.setUIMode = setUIMode;
window.renderTopicTabs = renderTopicTabs;
window.updateActiveTab = updateActiveTab;
window.hideTopicTabs = hideTopicTabs;
window.confirmDisableForumMode = confirmDisableForumMode;
window.autoResizeInput = autoResizeInput;
window.saveDraft = saveDraft;
window.handleInputKeydown = handleInputKeydown;
window.copyText = copyText;
window.scrollToMessage = scrollToMessage;
window.closeAllContextMenus = closeAllContextMenus;
window.closeAllModals = closeAllModals;
window.refreshCurrentView = refreshCurrentView;

console.log('Topics module loaded (Telegram-style v2.0)');
