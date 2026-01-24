(() => {
    const logBox = document.getElementById('logBox');
    const sessionState = document.getElementById('sessionState');
    const statsMeta = document.getElementById('statsMeta');
    const refreshStats = document.getElementById('refreshStats');

    const statUsers = document.getElementById('statUsers');
    const statOnline = document.getElementById('statOnline');
    const statMessages = document.getElementById('statMessages');
    const statBanned = document.getElementById('statBanned');
    const statActive = document.getElementById('statActive');
    const statGroups = document.getElementById('statGroups');
    const statChannels = document.getElementById('statChannels');
    const statBots = document.getElementById('statBots');
    const statReports = document.getElementById('statReports');

    const searchForm = document.getElementById('searchForm');
    const searchQuery = document.getElementById('searchQuery');
    const searchScope = document.getElementById('searchScope');
    const searchLimit = document.getElementById('searchLimit');
    const searchMeta = document.getElementById('searchMeta');
    const searchUsers = document.getElementById('searchUsers');
    const searchMessages = document.getElementById('searchMessages');
    const searchGroups = document.getElementById('searchGroups');
    const searchChannels = document.getElementById('searchChannels');

    const userLookupForm = document.getElementById('userLookupForm');
    const userLookupQuery = document.getElementById('userLookupQuery');
    const userLookupResult = document.getElementById('userLookupResult');
    const userIdInputs = document.querySelectorAll('.user-id-input');

    const autoModForm = document.getElementById('autoModForm');
    const autoModEnabled = document.getElementById('autoModEnabled');
    const autoModDelete = document.getElementById('autoModDelete');
    const autoModBan = document.getElementById('autoModBan');
    const autoModResolve = document.getElementById('autoModResolve');
    const autoModSpam = document.getElementById('autoModSpam');
    const autoModAbuse = document.getElementById('autoModAbuse');
    const autoModWindow = document.getElementById('autoModWindow');
    const autoModBanHours = document.getElementById('autoModBanHours');

    const premiumPaymentLookupForm = document.getElementById('premiumPaymentLookupForm');
    const premiumPaymentLabel = document.getElementById('premiumPaymentLabel');
    const premiumPaymentResult = document.getElementById('premiumPaymentResult');
    const premiumPaymentConfirmForm = document.getElementById('premiumPaymentConfirmForm');
    const premiumPaymentConfirmLabel = document.getElementById('premiumPaymentConfirmLabel');
    const premiumPaymentOperationId = document.getElementById('premiumPaymentOperationId');
    const premiumPaymentType = document.getElementById('premiumPaymentType');
    const premiumPaymentReactivateForm = document.getElementById('premiumPaymentReactivateForm');
    const premiumPaymentReactivateLabel = document.getElementById('premiumPaymentReactivateLabel');
    const premiumPaymentRefresh = document.getElementById('premiumPaymentRefresh');
    const premiumPaymentPendingList = document.getElementById('premiumPaymentPendingList');

    const log = (msg) => {
        const ts = new Date().toISOString();
        logBox.textContent = `[${ts}] ${msg}\n` + logBox.textContent;
    };

    const setSessionState = (active) => {
        sessionState.classList.toggle('active', active);
        sessionState.textContent = active ? 'Сессия активна' : 'Сессия закрыта';
    };

    const toggleControls = (enabled) => {
        document.querySelectorAll('input, textarea, button, select').forEach((el) => {
            el.disabled = !enabled;
        });
        statsMeta.textContent = enabled ? 'Данные актуальны' : 'Требуется вход';
    };

    const escapeHtml = (text) => {
        return String(text)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    };

    const highlight = (text, query) => {
        const safeText = escapeHtml(text || '');
        if (!query) return safeText;
        const safeQuery = String(query).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        return safeText.replace(new RegExp(safeQuery, 'ig'), (match) => `<span class="highlight">${match}</span>`);
    };

    const parseBool = (value, fallback = false) => {
        if (value === undefined || value === null || value === '') return fallback;
        const normalized = String(value).toLowerCase();
        return normalized === 'true' || normalized === '1' || normalized === 'yes' || normalized === 'on';
    };

    const statusTagClass = (status) => {
        const normalized = String(status || '').toLowerCase();
        if (normalized === 'paid') return 'positive';
        if (normalized === 'pending') return 'warning';
        if (normalized === 'failed' || normalized === 'canceled') return 'negative';
        return '';
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

    const applyStats = (data) => {
        if (!data) return;
        statUsers.textContent = data.users || '0';
        statOnline.textContent = data.online_users || '0';
        statMessages.textContent = data.messages_today || '0';
        statBanned.textContent = data.banned_users || '0';
        statActive.textContent = data.active_24h || '0';
        statGroups.textContent = data.groups || '0';
        statChannels.textContent = data.channels || '0';
        statBots.textContent = data.bots || '0';
        statReports.textContent = data.reports_pending || '0';
    };

    const fillUserIds = (userId) => {
        if (!userId) return;
        userIdInputs.forEach((input) => {
            input.value = userId;
        });
    };

    const runAdminAction = async (payload, label) => {
        const res = await apiPost('/api/admin/action', payload);
        const title = label || payload.action;
        if (res.success) {
            log(`${title}: ${res.message || 'ok'}`);
        } else {
            log(`${title}: ${res.message || res.error || 'error'}`);
        }
        return res;
    };

    const verifySession = async () => {
        const res = await apiPost('/api/admin/action', { action: 'stats' });
        if (!res.success) {
            window.location.href = '/admin';
            return;
        }
        setSessionState(true);
        toggleControls(true);
        if (res.data) {
            applyStats(res.data);
        }
        await loadAutoModeration();
        await loadPremiumPayments();
        await loadVerificationRequests();
        setupVerificationTabs();
    };

    const loadStats = async () => {
        const res = await apiPost('/api/admin/action', { action: 'stats' });
        if (res.success && res.data) {
            applyStats(res.data);
            statsMeta.textContent = 'Данные актуальны';
        } else {
            statsMeta.textContent = res.message || 'Не удалось загрузить статистику';
        }
    };

    const loadAutoModeration = async () => {
        const res = await apiPost('/api/admin/action', { action: 'auto_moderation_get' });
        if (!res.success || !res.data) {
            log(`auto_moderation_get: ${res.message || 'error'}`);
            return;
        }
        const data = res.data;
        autoModEnabled.checked = parseBool(data.enabled, false);
        autoModDelete.checked = parseBool(data.delete_on_threshold, true);
        autoModBan.checked = parseBool(data.ban_on_threshold, true);
        autoModResolve.checked = parseBool(data.auto_resolve, true);
        autoModSpam.value = data.spam_threshold || '3';
        autoModAbuse.value = data.abuse_threshold || '2';
        autoModWindow.value = data.window_minutes || '60';
        autoModBanHours.value = data.ban_hours || '24';
    };

    const renderUserLookup = (user) => {
        const isActive = parseBool(user.is_active, true);
        const isAdmin = parseBool(user.is_admin, false);
        const isBot = parseBool(user.is_bot, false);
        const tags = [
            `<span class="result-tag ${isActive ? 'positive' : 'negative'}">${isActive ? 'active' : 'banned'}</span>`,
            isAdmin ? '<span class="result-tag">admin</span>' : '',
            isBot ? '<span class="result-tag">bot</span>' : '',
            user.role ? `<span class="result-tag">${escapeHtml(user.role)}</span>` : ''
        ].filter(Boolean).join('');

        userLookupResult.innerHTML = `
            <div class="result-tags">${tags || '<span class="result-tag">user</span>'}</div>
            <div class="admin-kv">
                <div class="admin-kv-row"><span>Username</span><span>${escapeHtml(user.username || '—')}</span></div>
                <div class="admin-kv-row"><span>ID</span><span>${escapeHtml(user.id || '—')}</span></div>
                <div class="admin-kv-row"><span>Last login</span><span>${escapeHtml(user.last_login || '—')}</span></div>
                <div class="admin-kv-row"><span>Last activity</span><span>${escapeHtml(user.last_activity || '—')}</span></div>
                <div class="admin-kv-row"><span>Created</span><span>${escapeHtml(user.created_at || '—')}</span></div>
            </div>
        `;
        userLookupResult.classList.remove('admin-muted');
        fillUserIds(user.id);
    };

    const renderPremiumPaymentResult = (payment) => {
        if (!premiumPaymentResult) return;
        if (!payment) {
            premiumPaymentResult.textContent = 'Платеж не найден';
            premiumPaymentResult.classList.add('admin-muted');
            return;
        }
        const tags = [
            `<span class="result-tag ${statusTagClass(payment.status)}">${escapeHtml(payment.status || '—')}</span>`,
            payment.plan ? `<span class="result-tag">${escapeHtml(payment.plan)}</span>` : ''
        ].filter(Boolean).join('');

        premiumPaymentResult.innerHTML = `
            ${tags ? `<div class="result-tags">${tags}</div>` : ''}
            <div class="admin-kv">
                <div class="admin-kv-row"><span>Label</span><span>${escapeHtml(payment.label || '—')}</span></div>
                <div class="admin-kv-row"><span>Сумма</span><span>${escapeHtml(payment.amount || '—')} ₽</span></div>
                <div class="admin-kv-row"><span>Пользователь</span><span>${escapeHtml(payment.username || payment.user_id || '—')}</span></div>
                <div class="admin-kv-row"><span>Premium</span><span>${parseBool(payment.user_is_premium) ? 'active' : 'free'}</span></div>
                <div class="admin-kv-row"><span>План</span><span>${escapeHtml(payment.user_premium_plan || payment.plan || '—')}</span></div>
                <div class="admin-kv-row"><span>Создан</span><span>${escapeHtml(payment.created_at || '—')}</span></div>
                <div class="admin-kv-row"><span>Оплачен</span><span>${escapeHtml(payment.paid_at || '—')}</span></div>
                <div class="admin-kv-row"><span>Operation ID</span><span>${escapeHtml(payment.operation_id || '—')}</span></div>
                <div class="admin-kv-row"><span>Payment type</span><span>${escapeHtml(payment.payment_type || '—')}</span></div>
            </div>
        `;
        premiumPaymentResult.classList.remove('admin-muted');
        if (payment.label) {
            if (premiumPaymentLabel) premiumPaymentLabel.value = payment.label;
            if (premiumPaymentConfirmLabel) premiumPaymentConfirmLabel.value = payment.label;
            if (premiumPaymentReactivateLabel) premiumPaymentReactivateLabel.value = payment.label;
        }
    };

    const renderPremiumPaymentList = (payments) => {
        if (!premiumPaymentPendingList) return;
        if (!payments || !payments.length) {
            premiumPaymentPendingList.textContent = 'Пока пусто';
            premiumPaymentPendingList.classList.add('admin-muted');
            return;
        }
        premiumPaymentPendingList.classList.remove('admin-muted');
        premiumPaymentPendingList.innerHTML = payments.map((p) => {
            const tags = [
                `<span class="result-tag ${statusTagClass(p.status)}">${escapeHtml(p.status || '—')}</span>`,
                p.plan ? `<span class="result-tag">${escapeHtml(p.plan)}</span>` : ''
            ].filter(Boolean).join('');
            const metaParts = [
                `Пользователь: ${escapeHtml(p.username || p.user_id || '—')}`,
                `Сумма: ${escapeHtml(p.amount || '—')} ₽`,
                `Создан: ${escapeHtml(p.created_at || '—')}`
            ];
            return `
                <div class="result-item">
                    <div class="result-title">${escapeHtml(p.label || '—')}</div>
                    ${tags ? `<div class="result-tags">${tags}</div>` : ''}
                    <div class="result-meta">${metaParts.join(' · ')}</div>
                    <div class="result-actions">
                        <button class="btn-ghost btn-small" data-action="copy" data-copy="${escapeHtml(p.label || '')}">Скопировать label</button>
                        <button class="btn-secondary btn-small" data-action="premium_confirm" data-label="${escapeHtml(p.label || '')}">Подтвердить</button>
                    </div>
                </div>
            `;
        }).join('');
    };

    const loadPremiumPayments = async () => {
        if (!premiumPaymentPendingList) return;
        premiumPaymentPendingList.textContent = 'Загрузка...';
        premiumPaymentPendingList.classList.add('admin-muted');
        const res = await apiPost('/api/admin/action', {
            action: 'premium_payment_list',
            status: 'pending',
            limit: 20
        });
        if (!res.success) {
            premiumPaymentPendingList.textContent = res.message || 'Не удалось загрузить платежи';
            return;
        }
        renderPremiumPaymentList(res.payments || []);
    };

    const lookupPremiumPayment = async (label) => {
        if (!label || !premiumPaymentResult) return;
        const res = await apiPost('/api/admin/action', {
            action: 'premium_payment_lookup',
            label
        });
        if (res.success && res.data) {
            renderPremiumPaymentResult(res.data);
            log(`premium_payment_lookup: ${label}`);
        } else {
            premiumPaymentResult.textContent = res.message || 'Платеж не найден';
            premiumPaymentResult.classList.add('admin-muted');
            log(`premium_payment_lookup: ${res.message || res.error || 'error'}`);
        }
    };

    const renderSearchGroup = (container, items, renderItem, emptyLabel) => {
        container.innerHTML = '';
        if (!items || !items.length) {
            container.textContent = emptyLabel;
            container.classList.add('admin-muted');
            return;
        }
        container.classList.remove('admin-muted');
        const fragments = items.map(renderItem);
        container.innerHTML = fragments.join('');
    };

    const renderSearchResults = (data, query) => {
        const users = data.users || [];
        const messages = data.messages || [];
        const groups = data.groups || [];
        const channels = data.channels || [];

        renderSearchGroup(searchUsers, users, (u) => {
            const isActive = !!u.is_active;
            const isAdmin = !!u.is_admin;
            const isBot = !!u.is_bot;
            const statusTag = `<span class="result-tag ${isActive ? 'positive' : 'negative'}">${isActive ? 'active' : 'banned'}</span>`;
            const roleTag = u.role ? `<span class="result-tag">${escapeHtml(u.role)}</span>` : '';
            const adminTag = isAdmin ? '<span class="result-tag">admin</span>' : '';
            const botTag = isBot ? '<span class="result-tag">bot</span>' : '';
            const title = highlight(u.username || u.id, query);
            const meta = `${escapeHtml(u.id)} · ${escapeHtml(u.last_activity || '—')}`;
            return `
                <div class="result-item">
                    <div class="result-title">${title}</div>
                    <div class="result-tags">${statusTag}${roleTag}${adminTag}${botTag}</div>
                    <div class="result-meta">${meta}</div>
                    <div class="result-actions">
                        <button class="btn-ghost btn-small" data-action="use_user" data-user-id="${escapeHtml(u.id)}">Использовать</button>
                        <button class="btn-secondary btn-small" data-action="${isActive ? 'ban_user' : 'unban_user'}" data-user-id="${escapeHtml(u.id)}">${isActive ? 'Забанить' : 'Разбанить'}</button>
                        <button class="btn-ghost btn-small" data-action="force_logout" data-user-id="${escapeHtml(u.id)}">Сессии</button>
                        <button class="btn-ghost btn-small" data-action="set_admin" data-user-id="${escapeHtml(u.id)}" data-flag="${isAdmin ? 'false' : 'true'}">${isAdmin ? 'Снять админ' : 'Сделать админ'}</button>
                    </div>
                </div>
            `;
        }, 'Пользователи не найдены');

        renderSearchGroup(searchMessages, messages, (m) => {
            const content = highlight(m.content || '[нет текста]', query);
            const sender = m.sender_username || m.sender_id || '—';
            const receiver = m.receiver_username || m.receiver_id || '—';
            const meta = `${escapeHtml(sender)} → ${escapeHtml(receiver)}`;
            const time = escapeHtml(m.created_at || '');
            return `
                <div class="result-item">
                    <div class="result-title">${content}</div>
                    <div class="result-meta">${meta}</div>
                    <div class="result-meta">${time}</div>
                    <div class="result-actions">
                        <button class="btn-danger btn-small" data-action="delete_message_any" data-message-id="${escapeHtml(m.id)}">Удалить</button>
                    </div>
                </div>
            `;
        }, 'Сообщения не найдены');

        renderSearchGroup(searchGroups, groups, (g) => {
            const title = highlight(g.name || g.id, query);
            const desc = g.description ? highlight(g.description, query) : '';
            const meta = `ID: ${escapeHtml(g.id)} · Создатель: ${escapeHtml(g.creator_id || '—')}`;
            return `
                <div class="result-item">
                    <div class="result-title">${title}</div>
                    ${desc ? `<div class="result-meta">${desc}</div>` : ''}
                    <div class="result-meta">${meta}</div>
                    <div class="result-actions">
                        <button class="btn-ghost btn-small" data-action="copy" data-copy="${escapeHtml(g.id)}">Скопировать ID</button>
                    </div>
                </div>
            `;
        }, 'Группы не найдены');

        renderSearchGroup(searchChannels, channels, (c) => {
            const title = highlight(c.name || c.id, query);
            const desc = c.description ? highlight(c.description, query) : '';
            const meta = `ID: ${escapeHtml(c.id)} · Создатель: ${escapeHtml(c.creator_id || '—')}`;
            return `
                <div class="result-item">
                    <div class="result-title">${title}</div>
                    ${desc ? `<div class="result-meta">${desc}</div>` : ''}
                    <div class="result-meta">${meta}</div>
                    <div class="result-actions">
                        <button class="btn-ghost btn-small" data-action="copy" data-copy="${escapeHtml(c.id)}">Скопировать ID</button>
                    </div>
                </div>
            `;
        }, 'Каналы не найдены');

        const metaParts = [
            `users: ${users.length}`,
            `messages: ${messages.length}`,
            `groups: ${groups.length}`,
            `channels: ${channels.length}`
        ];
        searchMeta.textContent = `Найдено ${metaParts.join(' · ')}`;
    };

    const handleAction = (formId, builder) => {
        const form = document.getElementById(formId);
        if (!form) return;
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            const payload = builder();
            if (!payload) return;
            await runAdminAction(payload);
        });
    };

    toggleControls(false);
    setSessionState(false);

    handleAction('banForm', () => ({
        action: 'ban_user',
        user_id: document.getElementById('banUserId').value.trim()
    }));

    handleAction('unbanForm', () => ({
        action: 'unban_user',
        user_id: document.getElementById('unbanUserId').value.trim()
    }));

    handleAction('resetForm', () => ({
        action: 'reset_password',
        user_id: document.getElementById('resetUserId').value.trim(),
        new_password: document.getElementById('resetPassword').value
    }));

    handleAction('forceLogoutForm', () => ({
        action: 'user_force_logout',
        user_id: document.getElementById('forceLogoutUserId').value.trim()
    }));

    handleAction('roleForm', () => ({
        action: 'user_set_role',
        user_id: document.getElementById('roleUserId').value.trim(),
        role: document.getElementById('roleSelect').value
    }));

    handleAction('adminFlagForm', () => ({
        action: 'user_set_admin',
        user_id: document.getElementById('adminUserId').value.trim(),
        is_admin: document.getElementById('adminFlag').value
    }));

    handleAction('deleteMsgForm', () => ({
        action: 'delete_message_any',
        message_id: document.getElementById('messageId').value.trim(),
        message_type: document.getElementById('messageType').value
    }));

    handleAction('nukeForm', () => ({
        action: 'nuke_user_messages',
        user_id: document.getElementById('nukeUserId').value.trim()
    }));

    document.getElementById('broadcastForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = { action: 'broadcast', message: document.getElementById('broadcastText').value };
        await runAdminAction(payload, 'broadcast');
    });

    document.getElementById('restartBtn').addEventListener('click', async () => {
        await runAdminAction({ action: 'restart' }, 'restart');
    });

    userLookupForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const query = userLookupQuery.value.trim();
        if (!query) return;
        const res = await apiPost('/api/admin/action', { action: 'user_lookup', query });
        if (res.success && res.data) {
            renderUserLookup(res.data);
            log(`user_lookup: ${res.data.username || res.data.id || 'ok'}`);
        } else {
            userLookupResult.textContent = res.message || 'Пользователь не найден';
            userLookupResult.classList.add('admin-muted');
            log(`user_lookup: ${res.message || res.error || 'error'}`);
        }
    });

    searchForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const query = searchQuery.value.trim();
        if (!query) return;
        searchMeta.textContent = 'Поиск...';
        const res = await apiPost('/api/admin/action', {
            action: 'search',
            query,
            scope: searchScope.value,
            limit: searchLimit.value
        });
        if (res.success) {
            renderSearchResults(res, query);
        } else {
            searchMeta.textContent = res.message || 'Ошибка поиска';
        }
    });

    autoModForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            action: 'auto_moderation_set',
            enabled: autoModEnabled.checked,
            spam_threshold: autoModSpam.value,
            abuse_threshold: autoModAbuse.value,
            delete_on_threshold: autoModDelete.checked,
            ban_on_threshold: autoModBan.checked,
            ban_hours: autoModBanHours.value,
            window_minutes: autoModWindow.value,
            auto_resolve: autoModResolve.checked
        };
        await runAdminAction(payload, 'auto_moderation_set');
        await loadAutoModeration();
    });

    if (premiumPaymentLookupForm) {
        premiumPaymentLookupForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const label = premiumPaymentLabel?.value.trim();
            if (!label) return;
            await lookupPremiumPayment(label);
        });
    }

    if (premiumPaymentConfirmForm) {
        premiumPaymentConfirmForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const label = premiumPaymentConfirmLabel?.value.trim() || premiumPaymentLabel?.value.trim();
            if (!label) return;
            const payload = {
                action: 'premium_payment_confirm',
                label,
                operation_id: premiumPaymentOperationId?.value.trim() || '',
                payment_type: premiumPaymentType?.value.trim() || ''
            };
            const res = await runAdminAction(payload, 'premium_payment_confirm');
            if (res.success) {
                await lookupPremiumPayment(label);
                await loadPremiumPayments();
            }
        });
    }

    if (premiumPaymentReactivateForm) {
        premiumPaymentReactivateForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const label = premiumPaymentReactivateLabel?.value.trim() || premiumPaymentLabel?.value.trim();
            if (!label) return;
            const res = await runAdminAction({ action: 'premium_payment_reactivate', label }, 'premium_payment_reactivate');
            if (res.success) {
                await lookupPremiumPayment(label);
                await loadPremiumPayments();
            }
        });
    }

    if (premiumPaymentRefresh) {
        premiumPaymentRefresh.addEventListener('click', loadPremiumPayments);
    }

    document.addEventListener('click', async (event) => {
        const target = event.target.closest('[data-action]');
        if (!target) return;
        const action = target.dataset.action;
        if (!action) return;

        const userId = target.dataset.userId;
        const messageId = target.dataset.messageId;
        const copyValue = target.dataset.copy;

        if (action === 'use_user' && userId) {
            fillUserIds(userId);
            log(`user_selected: ${userId}`);
            return;
        }

        if (action === 'copy' && copyValue) {
            try {
                await navigator.clipboard.writeText(copyValue);
                log('copied to clipboard');
            } catch (e) {
                log('clipboard not available');
            }
            return;
        }

        target.disabled = true;
        try {
            if (action === 'premium_confirm' && target.dataset.label) {
                const label = target.dataset.label;
                const res = await runAdminAction({ action: 'premium_payment_confirm', label }, 'premium_payment_confirm');
                if (res.success) {
                    await lookupPremiumPayment(label);
                    await loadPremiumPayments();
                }
            } else if ((action === 'ban_user' || action === 'unban_user') && userId) {
                await runAdminAction({ action, user_id: userId }, action);
            } else if (action === 'force_logout' && userId) {
                await runAdminAction({ action: 'user_force_logout', user_id: userId }, 'force_logout');
            } else if (action === 'set_admin' && userId) {
                await runAdminAction({ action: 'user_set_admin', user_id: userId, is_admin: target.dataset.flag || 'false' }, 'set_admin');
            } else if (action === 'delete_message_any' && messageId) {
                await runAdminAction({ action: 'delete_message_any', message_id: messageId }, 'delete_message');
            }
        } finally {
            target.disabled = false;
        }
    });

    refreshStats.addEventListener('click', loadStats);

    // ========== Verification Requests ==========
    let currentVerifyStatus = 'pending';
    const verificationList = document.getElementById('verificationRequestsList');

    const setupVerificationTabs = () => {
        const tabs = document.querySelectorAll('.verify-tab');
        tabs.forEach(tab => {
            tab.addEventListener('click', async () => {
                tabs.forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                currentVerifyStatus = tab.dataset.status;
                await loadVerificationRequests();
            });
        });
    };

    const loadVerificationRequests = async () => {
        if (!verificationList) return;
        
        verificationList.innerHTML = '<div class="admin-muted">Загрузка...</div>';
        
        const token = localStorage.getItem('xipher_token');
        try {
            const res = await fetch('/api/get-verification-requests', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token, status: currentVerifyStatus })
            });
            const data = await res.json();
            
            if (!data.success || !data.requests || data.requests.length === 0) {
                verificationList.innerHTML = `
                    <div class="verify-empty">
                        <div class="verify-empty-icon">📋</div>
                        <div>Нет заявок${currentVerifyStatus === 'pending' ? ' на рассмотрении' : ''}</div>
                    </div>
                `;
                return;
            }
            
            verificationList.innerHTML = data.requests.map(req => renderVerificationRequest(req)).join('');
        } catch (error) {
            verificationList.innerHTML = '<div class="admin-muted">Ошибка загрузки</div>';
            log('verification_load_error: ' + error.message);
        }
    };

    const renderVerificationRequest = (req) => {
        const dateStr = new Date(req.created_at).toLocaleDateString('ru-RU', {
            day: 'numeric', month: 'short', year: 'numeric', hour: '2-digit', minute: '2-digit'
        });
        
        const isReviewed = req.status !== 'pending';
        const reviewedAt = req.reviewed_at ? new Date(req.reviewed_at).toLocaleDateString('ru-RU') : '';
        
        const actionsHtml = isReviewed 
            ? `<span class="verify-reviewed-badge ${req.status}">${req.status === 'approved' ? '✅ Одобрено' : '❌ Отклонено'}${reviewedAt ? ` (${reviewedAt})` : ''}</span>`
            : `
                <input type="text" class="verify-comment-input" placeholder="Комментарий (опционально)" data-request-id="${req.id}">
                <button class="verify-action-btn approve" data-action="approve" data-request-id="${req.id}">✓ Одобрить</button>
                <button class="verify-action-btn reject" data-action="reject" data-request-id="${req.id}">✗ Отклонить</button>
            `;
        
        const commentHtml = req.admin_comment 
            ? `<div class="verify-request-reason" style="margin-top: 8px;"><strong>Комментарий:</strong> ${escapeHtml(req.admin_comment)}</div>` 
            : '';

        return `
            <div class="verify-request-card" data-id="${req.id}">
                <div class="verify-request-avatar">${req.channel_name.charAt(0).toUpperCase()}</div>
                <div class="verify-request-body">
                    <div class="verify-request-header">
                        <span class="verify-request-channel">@${escapeHtml(req.channel_username)}</span>
                        <span class="verify-request-name">${escapeHtml(req.channel_name)}</span>
                    </div>
                    <div class="verify-request-stats">
                        <span>👥 ${formatNumber(req.subscribers_count)} подписчиков</span>
                        <span>👤 ${escapeHtml(req.owner_username || 'Неизвестно')}</span>
                        <span>📅 ${dateStr}</span>
                    </div>
                    ${req.reason ? `<div class="verify-request-reason">${escapeHtml(req.reason)}</div>` : ''}
                    ${commentHtml}
                    <div class="verify-request-actions">
                        ${actionsHtml}
                    </div>
                </div>
            </div>
        `;
    };

    const formatNumber = (num) => {
        if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
        if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
        return num.toString();
    };

    // Обработка кликов по кнопкам одобрения/отклонения
    document.addEventListener('click', async (event) => {
        const btn = event.target.closest('.verify-action-btn');
        if (!btn) return;
        
        const action = btn.dataset.action;
        const requestId = btn.dataset.requestId;
        if (!action || !requestId) return;
        
        const card = btn.closest('.verify-request-card');
        const commentInput = card?.querySelector('.verify-comment-input');
        const comment = commentInput?.value.trim() || '';
        
        btn.disabled = true;
        const originalText = btn.innerHTML;
        btn.innerHTML = '⏳';
        
        try {
            const token = localStorage.getItem('xipher_token');
            const res = await fetch('/api/review-verification', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token,
                    request_id: requestId,
                    action: action,
                    comment: comment
                })
            });
            const data = await res.json();
            
            if (data.success) {
                log(`verification_${action}: ${requestId}`);
                await loadVerificationRequests();
            } else {
                log(`verification_error: ${data.message}`);
                btn.innerHTML = originalText;
                btn.disabled = false;
            }
        } catch (error) {
            log('verification_action_error: ' + error.message);
            btn.innerHTML = originalText;
            btn.disabled = false;
        }
    });

    verifySession();
})();
