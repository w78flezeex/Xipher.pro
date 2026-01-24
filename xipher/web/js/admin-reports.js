const reportsBody = document.getElementById('reportsBody');
const detailPanel = document.getElementById('detailPanel');
const emptyState = document.getElementById('emptyState');
const paginationMeta = document.getElementById('paginationMeta');
const statusFilter = document.getElementById('statusFilter');
const toastEl = document.getElementById('toast');
const STATUS_LABELS = {
    pending: 'В ожидании',
    resolved: 'Решено',
    dismissed: 'Отклонено'
};

const store = {
    state: {
        reports: [],
        page: 1,
        pageSize: 10,
        total: 0,
        totalPages: 1,
        status: 'pending',
        selected: null,
        loading: false
    },
    listeners: [],
    set(partial) {
        this.state = { ...this.state, ...partial };
        this.listeners.forEach(fn => fn(this.state));
    },
    subscribe(fn) {
        this.listeners.push(fn);
        fn(this.state);
    }
};

function toast(message, type = 'info') {
    toastEl.textContent = message;
    toastEl.dataset.type = type === 'error' ? 'error' : 'info';
    toastEl.classList.add('show');
    clearTimeout(toastEl._timer);
    toastEl._timer = setTimeout(() => { toastEl.classList.remove('show'); }, 2200);
}

function showAuthRequired() {
    detailPanel.innerHTML = `<p class="admin-muted">Нет сессии. Авторизуйтесь в /login, затем в /admin.</p>`;
}

async function fetchReports() {
    store.set({ loading: true });
    try {
        const res = await fetch('/api/admin/reports/list', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'include',
            body: JSON.stringify({
                page: store.state.page,
                page_size: store.state.pageSize,
                status: store.state.status
            })
        });
        const json = await res.json();
        if (!json.success) {
            showAuthRequired();
            toast(json.error || json.message || 'Не удалось загрузить жалобы', 'error');
            store.set({ loading: false });
            return;
        }
        store.set({
            reports: json.items || [],
            total: json.total || 0,
            totalPages: json.total_pages || 1,
            selected: null,
            loading: false
        });
    } catch (e) {
        console.error(e);
        toast('Ошибка сети', 'error');
        store.set({ loading: false });
    }
}

async function updateReport(action, opts = {}) {
    const current = store.state.selected;
    if (!current) return;
    try {
        const payload = {
            report_id: current.id,
            action,
            resolution_note: opts.note || ''
        };
        if (opts.status) payload.status = opts.status;
        if (opts.banHours !== undefined) payload.ban_hours = opts.banHours;

        const res = await fetch('/api/admin/reports/update', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'include',
            body: JSON.stringify(payload)
        });
        const json = await res.json();
        if (!json.success) {
            showAuthRequired();
            toast(json.error || json.message || 'Не удалось применить действие', 'error');
            return;
        }
        toast('Обновлено');
        const remaining = store.state.reports.filter(r => r.id !== current.id);
        store.set({ reports: remaining, selected: null, total: Math.max(0, store.state.total - 1) });
        renderDetail(store.state);
        renderTable(store.state);
    } catch (e) {
        console.error(e);
        toast('Ошибка сети', 'error');
    }
}

function formatDate(dateStr) {
    if (!dateStr) return '—';
    const d = new Date(dateStr);
    return d.toLocaleString();
}

function renderTable(state) {
    reportsBody.innerHTML = '';
    if (!state.reports.length) {
        emptyState.style.display = 'block';
        return;
    }
    emptyState.style.display = 'none';

    state.reports.forEach(report => {
        const statusLabel = STATUS_LABELS[report.status] || report.status;
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><span class="status-pill ${report.status}">${statusLabel}</span></td>
            <td>${formatDate(report.created_at)}</td>
            <td>${report.reason}</td>
            <td>${report.reported_username || report.reported_user_id}</td>
            <td>${report.reporter_username || report.reporter_id}</td>
        `;
        tr.addEventListener('click', () => {
            store.set({ selected: report });
            renderDetail(store.state);
        });
        reportsBody.appendChild(tr);
    });
}

function renderDetail(state) {
    const r = state.selected;
    if (!r) {
        detailPanel.innerHTML = '<p class="admin-muted">Выберите жалобу</p>';
        return;
    }

    const context = Array.isArray(r.context) ? r.context : [];
    const statusLabel = STATUS_LABELS[r.status] || r.status;
    detailPanel.innerHTML = `
        <div class="pill" style="margin-bottom:8px;">
            <strong>${r.reason}</strong>
            <span class="status-pill ${r.status}">${statusLabel}</span>
        </div>
        <p><strong>Сообщение:</strong><br>${r.message_content || '—'}</p>
        <p class="admin-muted">ID сообщения: ${r.message_id}</p>
        <p><strong>Жалоба от:</strong> ${r.reporter_username || r.reporter_id}</p>
        <p><strong>Пользователь:</strong> ${r.reported_username || r.reported_user_id}</p>
        ${r.comment ? `<p><strong>Комментарий:</strong> ${r.comment}</p>` : ''}
        <div style="margin:12px 0;">
            <p style="margin-bottom:6px;"><strong>Контекст</strong></p>
            ${context.length ? `<ul class="context-list">
                ${context.map(c => `<li class="context-item"><div class="admin-muted">${formatDate(c.created_at)}</div><div>${c.content}</div><div class="admin-muted">${c.sender_id} → ${c.receiver_id}</div></li>`).join('')}
            </ul>` : '<p class="admin-muted">Контекст не найден</p>'}
        </div>
        <div class="actions">
            <button id="dismissBtn" class="btn-ghost btn-small">Отклонить</button>
            <button id="resolveBtn" class="btn-primary btn-small">Решено</button>
            <select id="banDuration" class="form-input" aria-label="Срок бана">
                <option value="0">Бан навсегда</option>
                <option value="24">24 часа</option>
                <option value="1">1 час</option>
            </select>
            <button id="banBtn" class="btn-danger btn-small">Забанить</button>
            <button id="deleteBtn" class="btn-secondary btn-small">Удалить сообщение</button>
        </div>
    `;

    document.getElementById('dismissBtn').onclick = () => updateReport('dismiss', { note: 'Отклонено админом' });
    document.getElementById('resolveBtn').onclick = () => updateReport('resolve', { status: 'resolved', note: 'Помечено как решено' });
    document.getElementById('deleteBtn').onclick = () => updateReport('delete_message', { note: 'Сообщение удалено' });
    document.getElementById('banBtn').onclick = () => {
        const banHours = parseInt(document.getElementById('banDuration').value, 10);
        const humanDuration = banHours ? `${banHours} ч.` : 'навсегда';
        updateReport('ban_user', { note: `Бан ${humanDuration}` });
    };
}

function render(state) {
    renderTable(state);
    renderDetail(state);
    paginationMeta.textContent = `Стр. ${state.page} из ${state.totalPages || 1} · Всего ${state.total}`;
}

store.subscribe(render);

document.getElementById('refreshBtn').addEventListener('click', () => {
    store.set({ page: 1 });
    fetchReports();
});

statusFilter.addEventListener('change', (e) => {
    store.set({ status: e.target.value, page: 1 });
    fetchReports();
});

document.getElementById('nextPage').addEventListener('click', () => {
    if (store.state.page < store.state.totalPages) {
        store.set({ page: store.state.page + 1 });
        fetchReports();
    }
});

document.getElementById('prevPage').addEventListener('click', () => {
    if (store.state.page > 1) {
        store.set({ page: store.state.page - 1 });
        fetchReports();
    }
});

fetchReports();
