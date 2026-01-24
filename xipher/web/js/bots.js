const API_BASE = '';
const PREMIUM_ACTIVE_KEY = 'xipher_premium_active';
let botsCache = [];
let revealBotId = '';
let editBotId = '';
let createFlowJsonOverride = '';
let createTemplateLabel = '';
let addToGroupBotUserId = '';
let addToGroupBotLabel = '';
let groupsCache = [];
let tplScope = 'all'; // all|dm|group
let tplCategory = 'all'; // all|<category>
let builderBotId = '';
let builderBot = null;
let builderMode = 'simple'; // simple|advanced

function getSessionToken() {
  return localStorage.getItem('xipher_token') || '';
}

async function requireAuthOrRedirect() {
  if (window.xipherSession) {
    const session = await window.xipherSession.requireSessionOrRedirect();
    return !!session;
  }
  const token = getSessionToken();
  const username = localStorage.getItem('xipher_username');
  if (!token || !username) {
    try { notifications.error('Необходима авторизация'); } catch (_) {}
    setTimeout(() => { window.location.href = '/login'; }, 600);
    return false;
  }
  return true;
}

function normalizeUsername(input) {
  let u = (input || '').trim();
  if (u.startsWith('@')) u = u.slice(1);
  return u.toLowerCase();
}

async function apiPost(path, body) {
  const res = await fetch(API_BASE + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body || {})
  });
  const text = await res.text();
  try { return JSON.parse(text); } catch (_) { return { success: false, message: text || 'Bad response' }; }
}

function normalizePremiumFlag(value) {
  return value === true || value === 'true';
}

function readPremiumCache() {
  try {
    const raw = localStorage.getItem(PREMIUM_ACTIVE_KEY);
    if (raw === 'true') return true;
    if (raw === 'false') return false;
  } catch (_) {}
  return null;
}

function cachePremiumState(active) {
  try {
    localStorage.setItem(PREMIUM_ACTIVE_KEY, active ? 'true' : 'false');
  } catch (_) {}
}

async function resolvePremiumStatus() {
  let payload = null;
  if (window.xipherSession && typeof window.xipherSession.validateSession === 'function') {
    try {
      const session = await window.xipherSession.validateSession();
      if (session && session.success) {
        payload = session.data || session.user || null;
      }
    } catch (_) {}
  }

  if (!payload) {
    try {
      const token = getSessionToken();
      const body = token && token !== 'cookie' ? { token } : {};
      const data = await apiPost('/api/validate-token', body);
      if (data && data.success) {
        payload = data.data || data.user || null;
      }
    } catch (_) {}
  }

  if (payload) {
    const active = normalizePremiumFlag(payload.is_premium);
    cachePremiumState(active);
    return active;
  }

  cachePremiumState(false);
  return false;
}

function setPremiumLocked(locked) {
  const lock = document.getElementById('premiumLock');
  if (lock) {
    lock.classList.toggle('active', locked);
  }
  if (document.body) {
    document.body.classList.toggle('premium-locked', locked);
  }
}

function renderBots(bots) {
  const list = document.getElementById('botsList');
  const emptyHint = document.getElementById('emptyHint');
  list.innerHTML = '';
  botsCache = bots || [];

  if (!bots || !bots.length) {
    emptyHint.style.display = 'block';
    return;
  }
  emptyHint.style.display = 'none';

  for (const b of bots) {
    const row = document.createElement('div');
    row.className = 'bot-row';

    const left = document.createElement('div');
    left.className = 'bot-left';

    const avatar = document.createElement('div');
    avatar.className = 'bot-avatar';
    if (b.bot_avatar_url) {
      const img = document.createElement('img');
      img.src = b.bot_avatar_url;
      img.onerror = () => { avatar.textContent = (b.bot_name || 'B').charAt(0).toUpperCase(); };
      avatar.appendChild(img);
    } else {
      avatar.textContent = (b.bot_name || 'B').charAt(0).toUpperCase();
    }

    const meta = document.createElement('div');
    meta.className = 'bot-meta';
    meta.innerHTML = `
      <div class="bot-name">${escapeHtml(b.bot_name || 'Bot')}</div>
      <div class="bot-username">@${escapeHtml(b.bot_username || '')}</div>
      <div class="bot-state">${escapeHtml(b.bot_description || '')}</div>
    `;

    const actions = document.createElement('div');
    actions.className = 'row-actions';

    const openBtn = document.createElement('a');
    openBtn.className = 'btn';
    openBtn.innerHTML = '<i class="fas fa-comment"></i> <span>Открыть</span>';
    openBtn.href = b.bot_username ? `/@${encodeURIComponent(b.bot_username)}` : '/chat';
    openBtn.title = `@${escapeHtml(b.bot_username || '')}`;
    actions.appendChild(openBtn);

    const editBtn = document.createElement('button');
    editBtn.className = 'btn';
    editBtn.innerHTML = '<i class="fas fa-edit"></i> <span>Редактировать</span>';
    editBtn.addEventListener('click', () => openEditModal(b));
    actions.appendChild(editBtn);

    const buildBtn = document.createElement('button');
    buildBtn.className = 'btn';
    buildBtn.innerHTML = '<i class="fas fa-cogs"></i> <span>Конструктор</span>';
    buildBtn.addEventListener('click', () => openBuilderModal(b));
    actions.appendChild(buildBtn);

    const ideBtn = document.createElement('button');
    ideBtn.className = 'btn';
    ideBtn.innerHTML = '<i class="fas fa-code"></i> <span>IDE</span>';
    ideBtn.addEventListener('click', () => {
      window.location.href = `/bot-ide.html?bot_id=${encodeURIComponent(b.id)}`;
    });
    actions.appendChild(ideBtn);

    const addToGroupBtn = document.createElement('button');
    addToGroupBtn.className = 'btn';
    addToGroupBtn.innerHTML = '<i class="fas fa-users"></i> <span>В группу</span>';
    addToGroupBtn.addEventListener('click', () => openAddToGroupModal(b));
    actions.appendChild(addToGroupBtn);

    const revealBtn = document.createElement('button');
    revealBtn.className = 'btn';
    revealBtn.innerHTML = '<i class="fas fa-key"></i> <span>Показать токен</span>';
    revealBtn.addEventListener('click', () => openRevealModal(b));
    actions.appendChild(revealBtn);

    const apiLink = document.createElement('a');
    apiLink.className = 'btn';
    apiLink.href = '/api';
    apiLink.innerHTML = '<i class="fas fa-book"></i> <span>API</span>';
    actions.appendChild(apiLink);

    const developersBtn = document.createElement('button');
    developersBtn.className = 'btn';
    developersBtn.innerHTML = '<i class="fas fa-user-friends"></i> <span>Разработчики</span>';
    developersBtn.addEventListener('click', () => openDevelopersModal(b));
    actions.appendChild(developersBtn);

    const deleteBtn = document.createElement('button');
    deleteBtn.className = 'btn';
    deleteBtn.style.color = '#ef4444';
    deleteBtn.innerHTML = '<i class="fas fa-trash"></i> <span>Удалить</span>';
    deleteBtn.addEventListener('click', () => openDeleteModal(b));
    actions.appendChild(deleteBtn);

    left.appendChild(avatar);
    left.appendChild(meta);
    row.appendChild(left);
    row.appendChild(actions);
    list.appendChild(row);
  }
}

function escapeHtml(str) {
  return String(str || '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

async function loadBots() {
  const token = getSessionToken();
  const data = await apiPost('/api/get-user-bots', { token });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось загрузить ботов');
    return;
  }
  renderBots(data.bots || []);
}

function decodeMultiline(s) {
  return String(s || '').replaceAll('\\n', '\n').replaceAll('\\t', '\t');
}
function encodeMultiline(s) {
  return String(s || '').replaceAll('\r\n', '\n').replaceAll('\n', '\\n').replaceAll('\t', '\\t');
}

function setPanelVisibility() {
  const rulesOn = document.getElementById('cfgRules').checked;
  const welcomeOn = document.getElementById('cfgWelcome').checked;
  const modOn = document.getElementById('cfgModeration').checked;
  const arOn = document.getElementById('cfgAutoReply').checked;
  document.getElementById('rulesBox').style.display = rulesOn ? 'block' : 'none';
  document.getElementById('welcomeBox').style.display = welcomeOn ? 'block' : 'none';
  document.getElementById('moderationBox').style.display = modOn ? 'block' : 'none';
  document.getElementById('autoreplyBox').style.display = arOn ? 'block' : 'none';
}

function parseFlowJsonSafe(flowJson) {
  const raw = String(flowJson || '').trim();
  if (!raw) return { ok: true, obj: {} };
  try {
    const obj = JSON.parse(raw);
    return { ok: true, obj };
  } catch (e) {
    return { ok: false, error: e?.message || 'JSON parse error' };
  }
}

function isNodeGraph(obj) {
  return obj && typeof obj === 'object' && (Array.isArray(obj.nodes) || Array.isArray(obj.edges));
}

function renderAutoReplyList(pairs) {
  const wrap = document.getElementById('cfgAutoReplyList');
  wrap.innerHTML = '';
  const list = Array.isArray(pairs) ? pairs : [];
  if (!list.length) {
    const hint = document.createElement('div');
    hint.className = 'hint';
    hint.textContent = 'Пока нет правил.';
    wrap.appendChild(hint);
  }
  for (let i = 0; i < list.length; i++) {
    const row = document.createElement('div');
    row.className = 'kv-row';
    const k = document.createElement('input');
    k.placeholder = 'Ключ (напр. оплата)';
    k.value = list[i].key || '';
    const v = document.createElement('input');
    v.placeholder = 'Ответ';
    v.value = list[i].value || '';
    const del = document.createElement('button');
    del.className = 'btn';
    del.textContent = 'Удалить';
    del.addEventListener('click', () => {
      list.splice(i, 1);
      renderAutoReplyList(list);
    });
    row.appendChild(k);
    row.appendChild(v);
    row.appendChild(del);
    wrap.appendChild(row);
  }
  // store on element for easy retrieval
  wrap._pairs = list;
}

function renderCmdList(items) {
  const wrap = document.getElementById('cfgCmdList');
  wrap.innerHTML = '';
  const list = Array.isArray(items) ? items : [];
  if (!list.length) {
    const hint = document.createElement('div');
    hint.className = 'hint';
    hint.textContent = 'Пока нет команд.';
    wrap.appendChild(hint);
  }
  for (let i = 0; i < list.length; i++) {
    const row = document.createElement('div');
    row.className = 'kv-row';
    const cmd = document.createElement('input');
    cmd.placeholder = 'команда (без /)';
    cmd.value = list[i].cmd || '';
    const reply = document.createElement('textarea');
    reply.placeholder = 'ответ (любой текст)';
    reply.value = list[i].reply || '';
    const del = document.createElement('button');
    del.className = 'btn';
    del.textContent = 'Удалить';
    del.addEventListener('click', () => {
      list.splice(i, 1);
      renderCmdList(list);
    });
    row.appendChild(cmd);
    row.appendChild(reply);
    row.appendChild(del);
    wrap.appendChild(row);
  }
  wrap._items = list;
}

function collectCmdList() {
  const wrap = document.getElementById('cfgCmdList');
  const rows = Array.from(wrap.querySelectorAll('.kv-row'));
  const out = [];
  for (const r of rows) {
    const cmd = (r.querySelector('input')?.value || '').trim().toLowerCase().replace(/^\//, '');
    const reply = (r.querySelector('textarea')?.value || '').trim();
    if (!cmd || !reply) continue;
    // allow only safe key names in cfg: cmd_<name>
    if (!/^[a-z0-9_]{1,32}$/.test(cmd)) continue;
    out.push({ cmd, reply });
  }
  return out;
}

function collectAutoReplyRules() {
  const wrap = document.getElementById('cfgAutoReplyList');
  const rows = Array.from(wrap.querySelectorAll('.kv-row'));
  const pairs = [];
  for (const r of rows) {
    const inputs = r.querySelectorAll('input');
    if (!inputs || inputs.length < 2) continue;
    const key = (inputs[0].value || '').trim();
    const value = (inputs[1].value || '').trim();
    if (!key || !value) continue;
    // prevent breaking the simple "k=v;k2=v2" format
    if (key.includes('=') || key.includes(';')) continue;
    if (value.includes(';')) continue;
    pairs.push({ key, value });
  }
  return pairs;
}

function pairsToRuleString(pairs) {
  return (pairs || []).map(p => `${p.key}=${p.value}`).join(';');
}

function ruleStringToPairs(s) {
  const out = [];
  const raw = String(s || '').trim();
  if (!raw) return out;
  const items = raw.split(';');
  for (const item of items) {
    const idx = item.indexOf('=');
    if (idx <= 0) continue;
    const key = item.slice(0, idx).trim();
    const value = item.slice(idx + 1).trim();
    if (key && value) out.push({ key, value });
  }
  return out;
}

function openBuilderModal(bot) {
  builderBotId = bot?.id || '';
  builderBot = bot || null;
  if (!builderBotId) return notifications.error('Бот не выбран');

  document.getElementById('builderBotLabel').textContent = bot ? `${bot.bot_name || 'Bot'} (@${bot.bot_username || ''})` : '—';
  document.getElementById('builderBackdrop').style.display = 'flex';

  // tabs state
  builderMode = 'simple';
  document.querySelectorAll('#builderTabs .tab').forEach(t => t.classList.remove('active'));
  document.querySelector('#builderTabs .tab[data-tab="simple"]').classList.add('active');
  document.getElementById('panelSimple').classList.add('active');
  document.getElementById('panelAdvanced').classList.remove('active');
  document.getElementById('panelPython').classList.remove('active');

  // parse config
  const parsed = parseFlowJsonSafe(bot.flow_json);
  if (!parsed.ok) {
    notifications.error('flow_json сломан: ' + parsed.error);
    builderMode = 'advanced';
  }
  const obj = parsed.ok ? (parsed.obj || {}) : {};

  // If old nodes/edges bot: force advanced to avoid destroying
  if (isNodeGraph(obj)) {
    notifications.warning('Этот бот в продвинутом режиме (nodes/edges). Лучше править во вкладке “Код (JSON)”.');
    builderMode = 'advanced';
  }

  // Fill advanced editor always
  try {
    document.getElementById('cfgRawJson').value = JSON.stringify(obj, null, 2);
  } catch (_) {
    document.getElementById('cfgRawJson').value = String(bot.flow_json || '');
  }

  // Load python script (separately)
  (async () => {
    try {
      const token = getSessionToken();
      const resp = await apiPost('/api/get-bot-script', { token, bot_id: builderBotId });
      const d = resp && resp.success ? resp.data : null;
      document.getElementById('pyEnabled').checked = d ? (d.script_enabled === 'true') : false;
      document.getElementById('pyCode').value = d ? (d.script_code || '') : '';
    } catch (_) {
      // ignore
    }
  })();

  // Fill simple editor defaults
  const cfg = obj && typeof obj === 'object' ? obj : {};
  document.getElementById('cfgIsActive').checked = bot.is_active !== false && bot.is_active !== 'false';
  document.getElementById('cfgNotes').checked = String(cfg.module_notes || '') === 'true';
  document.getElementById('cfgRemind').checked = String(cfg.module_remind || '') === 'true';
  document.getElementById('cfgRules').checked = String(cfg.module_rules || '') === 'true';
  document.getElementById('cfgWelcome').checked = String(cfg.module_welcome || '') === 'true';
  document.getElementById('cfgModeration').checked = String(cfg.module_moderation || '') === 'true';
  document.getElementById('cfgAutoReply').checked = String(cfg.module_autoreply || '') === 'true';
  document.getElementById('cfgFun').checked = String(cfg.module_fun || '') === 'true';

  document.getElementById('cfgDmDefault').value = decodeMultiline(cfg.dm_default_reply || '');
  document.getElementById('cfgHelpText').value = decodeMultiline(cfg.help_text || '');
  document.getElementById('cfgUnknownCmd').value = decodeMultiline(cfg.unknown_command_reply || '');
  document.getElementById('cfgRulesText').value = decodeMultiline(cfg.rules_text || '');
  document.getElementById('cfgWelcomeText').value = decodeMultiline(cfg.welcome_text || '');

  document.getElementById('cfgModLinks').checked = String(cfg.mod_block_links || '') === 'true';
  document.getElementById('cfgModCaps').checked = String(cfg.mod_block_caps || '') === 'true';
  document.getElementById('cfgModBadWords').checked = String(cfg.mod_block_words || '') === 'true';
  document.getElementById('cfgModAutoMute').checked = String(cfg.mod_auto_mute || '') === 'true';
  document.getElementById('cfgModWords').value = String(cfg.mod_bad_words || '');
  document.getElementById('cfgModWarn').value = decodeMultiline(cfg.mod_warn_text || '⚠️ {reason}');

  renderAutoReplyList(ruleStringToPairs(cfg.autoreply_rules || ''));

  // custom commands: cfg keys cmd_<name> => reply
  const cmdItems = [];
  for (const [k, v] of Object.entries(cfg)) {
    if (!k || typeof k !== 'string') continue;
    if (!k.startsWith('cmd_')) continue;
    const name = k.slice(4);
    if (!name) continue;
    cmdItems.push({ cmd: name, reply: decodeMultiline(String(v || '')) });
  }
  cmdItems.sort((a, b) => a.cmd.localeCompare(b.cmd));
  renderCmdList(cmdItems);

  setPanelVisibility();

  // toggle UI if advanced
  if (builderMode === 'advanced') {
    document.querySelectorAll('#builderTabs .tab').forEach(t => t.classList.remove('active'));
    document.querySelector('#builderTabs .tab[data-tab="advanced"]').classList.add('active');
    document.getElementById('panelSimple').classList.remove('active');
    document.getElementById('panelAdvanced').classList.add('active');
  }
}

function closeBuilderModal() {
  document.getElementById('builderBackdrop').style.display = 'none';
  builderBotId = '';
  builderBot = null;
}

async function saveBuilder() {
  const token = getSessionToken();
  if (!builderBotId) return notifications.error('Бот не выбран');

  // Determine active tab
  const activeTab = document.querySelector('#builderTabs .tab.active')?.dataset?.tab || 'simple';

  let flowObj = {};
  if (activeTab === 'advanced') {
    const raw = document.getElementById('cfgRawJson').value || '';
    try {
      flowObj = JSON.parse(raw);
    } catch (e) {
      return notifications.error('JSON невалидный: ' + (e?.message || 'ошибка'));
    }
  } else if (activeTab === 'simple') {
    // Simple -> flat config, but preserve unknown keys by merging with existing cfg
    const parsedExisting = parseFlowJsonSafe(builderBot?.flow_json || '');
    flowObj = parsedExisting.ok ? (parsedExisting.obj || {}) : {};

    // wipe old custom commands
    for (const k of Object.keys(flowObj)) {
      if (k.startsWith('cmd_')) delete flowObj[k];
    }

    flowObj.template_id = (flowObj.template_id || 'custom');
    flowObj.module_notes = document.getElementById('cfgNotes').checked ? 'true' : 'false';
    flowObj.module_remind = document.getElementById('cfgRemind').checked ? 'true' : 'false';
    flowObj.module_rules = document.getElementById('cfgRules').checked ? 'true' : 'false';
    flowObj.module_welcome = document.getElementById('cfgWelcome').checked ? 'true' : 'false';
    flowObj.module_moderation = document.getElementById('cfgModeration').checked ? 'true' : 'false';
    flowObj.module_autoreply = document.getElementById('cfgAutoReply').checked ? 'true' : 'false';
    flowObj.module_fun = document.getElementById('cfgFun').checked ? 'true' : 'false';

    flowObj.dm_default_reply = encodeMultiline(document.getElementById('cfgDmDefault').value || '');
    flowObj.help_text = encodeMultiline(document.getElementById('cfgHelpText').value || '');
    flowObj.unknown_command_reply = encodeMultiline(document.getElementById('cfgUnknownCmd').value || '');
    flowObj.rules_text = encodeMultiline(document.getElementById('cfgRulesText').value || '');
    flowObj.welcome_text = encodeMultiline(document.getElementById('cfgWelcomeText').value || '');

    flowObj.mod_block_links = document.getElementById('cfgModLinks').checked ? 'true' : 'false';
    flowObj.mod_block_caps = document.getElementById('cfgModCaps').checked ? 'true' : 'false';
    flowObj.mod_block_words = document.getElementById('cfgModBadWords').checked ? 'true' : 'false';
    flowObj.mod_auto_mute = document.getElementById('cfgModAutoMute').checked ? 'true' : 'false';
    flowObj.mod_bad_words = (document.getElementById('cfgModWords').value || '').trim();
    flowObj.mod_warn_text = encodeMultiline(document.getElementById('cfgModWarn').value || '⚠️ {reason}');

    const pairs = collectAutoReplyRules();
    flowObj.autoreply_rules = pairsToRuleString(pairs);

    // add custom commands
    const cmds = collectCmdList();
    for (const item of cmds) {
      flowObj['cmd_' + item.cmd] = encodeMultiline(item.reply);
    }
  } else if (activeTab === 'python') {
    // Keep existing flow_json as-is when editing python (to avoid overwriting configs)
    const parsed = parseFlowJsonSafe(builderBot?.flow_json || '');
    flowObj = parsed.ok ? (parsed.obj || {}) : {};
  }

  const is_active = document.getElementById('cfgIsActive').checked ? 'true' : 'false';
  const flow_json = JSON.stringify(flowObj);

  const resp = await apiPost('/api/update-bot-flow', { token, bot_id: builderBotId, flow_json, is_active });
  if (!resp || !resp.success) {
    notifications.error(resp?.message || 'Не удалось сохранить');
    return;
  }

  // Save Python script settings (optional)
  const pyEnabled = document.getElementById('pyEnabled').checked;
  const pyCode = document.getElementById('pyCode').value || '';
  const pyResp = await apiPost('/api/update-bot-script', {
    token,
    bot_id: builderBotId,
    script_lang: 'python',
    script_enabled: pyEnabled ? 'true' : 'false',
    script_code: pyCode
  });
  if (!pyResp || !pyResp.success) {
    // Don't block flow save; just notify
    notifications.warning(pyResp?.message || 'Скрипт не сохранён');
  }

  notifications.success('Сохранено');
  closeBuilderModal();
  await loadBots();
}

function openCreateModal() {
  // opening manually: reset template override
  createFlowJsonOverride = '';
  createTemplateLabel = '';
  const hint = document.getElementById('createTemplateHint');
  hint.style.display = 'none';
  hint.textContent = '';
  document.getElementById('createBackdrop').style.display = 'flex';
  document.getElementById('tokenBox').style.display = 'none';
  document.getElementById('createdToken').textContent = '';
}

function closeCreateModal() {
  document.getElementById('createBackdrop').style.display = 'none';
}

async function createBot() {
  const token = getSessionToken();
  const bot_name = (document.getElementById('botName').value || '').trim();
  const bot_username = normalizeUsername(document.getElementById('botUsername').value || '');

  if (!bot_name) return notifications.error('Укажи название бота');
  if (!bot_username) return notifications.error('Укажи username бота');

  const payload = {
    token,
    bot_name,
    bot_username,
    flow_json: createFlowJsonOverride || '{"nodes":[],"edges":[]}'
  };

  const data = await apiPost('/api/create-bot', payload);
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось создать бота');
    return;
  }

  // reset template override for the next creation
  createFlowJsonOverride = '';
  createTemplateLabel = '';
  {
    const hint = document.getElementById('createTemplateHint');
    hint.style.display = 'none';
    hint.textContent = '';
  }

  // Show token once
  document.getElementById('createdToken').textContent = data.bot_token || '';
  document.getElementById('tokenBox').style.display = 'block';
  notifications.success('Бот создан');
  await loadBots();
}

function randomHex4() {
  return Math.floor(Math.random() * 0xffff).toString(16).padStart(4, '0');
}

function suggestUsernameFromTemplateId(tplId) {
  const base = normalizeUsername(String(tplId || 'xipher')) || 'xipher';
  // Ensure ends with "bot"
  let core = base.endsWith('bot') ? base.slice(0, -3) : base;
  core = core.replace(/[^a-z0-9_]/g, '_').replace(/__+/g, '_').replace(/^_+|_+$/g, '');
  const suffix = `${randomHex4()}_bot`;
  const maxCore = Math.max(1, 32 - (suffix.length + 1)); // +1 for underscore
  if (core.length > maxCore) core = core.slice(0, maxCore);
  let out = `${core}_${suffix}`;
  if (!out.endsWith('bot')) out += 'bot';
  if (out.length < 5) out = `xipher_${suffix}`;
  if (out.length > 32) out = out.slice(0, 32);
  // hard ensure ending
  if (!out.endsWith('bot')) out = out.slice(0, 29) + 'bot';
  return out;
}

function renderTemplates(templates) {
  const grid = document.getElementById('tplGrid');
  const empty = document.getElementById('tplEmpty');
  grid.innerHTML = '';
  const list = templates || [];
  if (!list.length) {
    empty.style.display = 'block';
    return;
  }
  empty.style.display = 'none';

  // Group by category
  const byCat = new Map();
  for (const t of list) {
    const cat = t.category || 'Другое';
    if (!byCat.has(cat)) byCat.set(cat, []);
    byCat.get(cat).push(t);
  }

  const orderedCats = Array.from(byCat.keys()).sort((a, b) => a.localeCompare(b, 'ru'));
  for (const cat of orderedCats) {
    const section = document.createElement('div');
    section.className = 'tpl-section';
    const title = document.createElement('div');
    title.className = 'tpl-section-title';
    title.innerHTML = `<span>${escapeHtml(cat)}</span><small>${byCat.get(cat).length} шт.</small>`;
    section.appendChild(title);

    const sectionGrid = document.createElement('div');
    sectionGrid.className = 'tpl-grid';

    for (const t of byCat.get(cat)) {
      const scopes = Array.isArray(t.scopes) ? t.scopes : [];
      const scopeTag = scopes.includes('dm') && scopes.includes('group') ? 'ЛС+Группа' : (scopes.includes('dm') ? 'ЛС' : 'Группа');

      const card = document.createElement('div');
      card.className = 'tpl-card';
      card.innerHTML = `
        <div class="tpl-title">${escapeHtml(t.title || 'Template')}</div>
        <div class="tpl-desc">${escapeHtml(t.description || '')}</div>
        <div class="tpl-tags">
          <span class="tag">${escapeHtml(cat)}</span>
          <span class="tag">${escapeHtml(scopeTag)}</span>
          <span class="tag">light</span>
        </div>
      `;
      const btnRow = document.createElement('div');
      btnRow.style.display = 'flex';
      btnRow.style.gap = '8px';
      btnRow.style.marginTop = '6px';

      const installBtn = document.createElement('button');
      installBtn.className = 'btn primary';
      installBtn.textContent = 'Создать';
      installBtn.addEventListener('click', () => {
        createFlowJsonOverride = JSON.stringify(t.config || {});
        createTemplateLabel = `${t.title || 'Шаблон'} • ${scopeTag}`;
        document.getElementById('botName').value = t.bot_name || t.title || 'Bot';
        document.getElementById('botUsername').value = suggestUsernameFromTemplateId(t.id || t.title);

        // open create modal without clearing override
        document.getElementById('createBackdrop').style.display = 'flex';
        document.getElementById('tokenBox').style.display = 'none';
        document.getElementById('createdToken').textContent = '';
        const hint = document.getElementById('createTemplateHint');
        hint.style.display = 'block';
        hint.textContent = `Выбран шаблон: ${createTemplateLabel}`;

        notifications.success('Шаблон выбран — проверь имя и username');
      });
      btnRow.appendChild(installBtn);

      const viewBtn = document.createElement('button');
      viewBtn.className = 'btn';
      viewBtn.textContent = 'Команды';
      viewBtn.addEventListener('click', () => {
        // show compact help based on modules
        const c = t.config || {};
        const lines = [];
        lines.push('Команды: /help, /start');
        if (c.module_rules === 'true') lines.push('/rules');
        if (c.module_notes === 'true') lines.push('/note, /notes, /delnote');
        if (c.module_remind === 'true') lines.push('/remind 10m текст');
        if (c.module_fun === 'true') lines.push('/roll, /coin, /choose');
        notifications.info(lines.join(' • '));
      });
      btnRow.appendChild(viewBtn);

      card.appendChild(btnRow);
      sectionGrid.appendChild(card);
    }

    section.appendChild(sectionGrid);
    grid.appendChild(section);
  }
}

function setupTemplatesUI() {
  const all = Array.isArray(window.XIPHER_BOT_TEMPLATES) ? window.XIPHER_BOT_TEMPLATES : [];
  // build filter chips
  const scopeWrap = document.getElementById('tplScopeFilters');
  const catWrap = document.getElementById('tplCategoryFilters');
  scopeWrap.innerHTML = '';
  catWrap.innerHTML = '';

  const scopeOptions = [
    { id: 'all', label: 'Все' },
    { id: 'dm', label: 'ЛС' },
    { id: 'group', label: 'Группы' }
  ];
  for (const s of scopeOptions) {
    const chip = document.createElement('div');
    chip.className = 'chip' + (tplScope === s.id ? ' active' : '');
    chip.textContent = s.label;
    chip.addEventListener('click', () => {
      tplScope = s.id;
      for (const c of scopeWrap.querySelectorAll('.chip')) c.classList.remove('active');
      chip.classList.add('active');
      applyTemplateFilters();
    });
    scopeWrap.appendChild(chip);
  }

  const cats = Array.from(new Set(all.map(t => t.category || 'Другое'))).sort((a, b) => a.localeCompare(b, 'ru'));
  const catOptions = ['all', ...cats];
  for (const c of catOptions) {
    const chip = document.createElement('div');
    chip.className = 'chip' + ((tplCategory === c) ? ' active' : '');
    chip.textContent = (c === 'all') ? 'Категории: все' : c;
    chip.addEventListener('click', () => {
      tplCategory = c;
      for (const x of catWrap.querySelectorAll('.chip')) x.classList.remove('active');
      chip.classList.add('active');
      applyTemplateFilters();
    });
    catWrap.appendChild(chip);
  }

  function applyTemplateFilters() {
    const q = (document.getElementById('tplSearch').value || '').trim().toLowerCase();
    const filtered = all.filter(t => {
      // category filter
      if (tplCategory !== 'all' && (t.category || 'Другое') !== tplCategory) return false;
      // scope filter
      if (tplScope !== 'all') {
        const scopes = Array.isArray(t.scopes) ? t.scopes : [];
        if (!scopes.includes(tplScope)) return false;
      }
      // search
      if (!q) return true;
      const hay = `${t.title || ''} ${t.description || ''} ${t.category || ''} ${t.id || ''}`.toLowerCase();
      return hay.includes(q);
    });
    renderTemplates(filtered);
  }

  // initial render
  renderTemplates(all);
  const input = document.getElementById('tplSearch');
  input.addEventListener('input', () => {
    applyTemplateFilters();
  });
}

async function openAddToGroupModal(bot) {
  addToGroupBotUserId = bot?.bot_user_id || '';
  addToGroupBotLabel = bot ? `${bot.bot_name || 'Bot'} (@${bot.bot_username || ''})` : '—';
  if (!addToGroupBotUserId) {
    return notifications.error('У этого бота нет bot_user_id (обнови страницу /bots)');
  }
  document.getElementById('addToGroupBotLabel').textContent = addToGroupBotLabel;
  document.getElementById('addToGroupBackdrop').style.display = 'flex';

  // load groups
  const token = getSessionToken();
  const data = await apiPost('/api/get-groups', { token });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось загрузить группы');
    return;
  }
  groupsCache = data.groups || [];
  const sel = document.getElementById('groupSelect');
  sel.innerHTML = '';
  if (!groupsCache.length) {
    const opt = document.createElement('option');
    opt.value = '';
    opt.textContent = 'Нет групп';
    sel.appendChild(opt);
    sel.disabled = true;
  } else {
    sel.disabled = false;
    const opt0 = document.createElement('option');
    opt0.value = '';
    opt0.textContent = 'Выбери группу…';
    sel.appendChild(opt0);
    for (const g of groupsCache) {
      const opt = document.createElement('option');
      opt.value = g.id;
      opt.textContent = g.name || g.id;
      sel.appendChild(opt);
    }
  }
}

function closeAddToGroupModal() {
  document.getElementById('addToGroupBackdrop').style.display = 'none';
  addToGroupBotUserId = '';
  addToGroupBotLabel = '';
}

async function confirmAddToGroup() {
  const token = getSessionToken();
  const group_id = document.getElementById('groupSelect').value || '';
  if (!addToGroupBotUserId) return notifications.error('Бот не выбран');
  if (!group_id) return notifications.error('Выбери группу');
  const data = await apiPost('/api/add-friend-to-group', { token, group_id, friend_id: addToGroupBotUserId });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось добавить бота');
    return;
  }
  notifications.success('Бот добавлен в группу');
  closeAddToGroupModal();
}

async function copyCreatedToken() {
  const token = document.getElementById('createdToken').textContent || '';
  if (!token) return;
  try {
    await navigator.clipboard.writeText(token);
    notifications.success('Токен скопирован');
  } catch (_) {
    notifications.error('Не удалось скопировать (браузер запретил)');
  }
}

function openRevealModal(bot) {
  revealBotId = bot?.id || '';
  document.getElementById('revealPassword').value = '';
  document.getElementById('revealTokenBox').style.display = 'none';
  document.getElementById('revealToken').textContent = '';
  const label = bot ? `${bot.bot_name || 'Bot'} (@${bot.bot_username || ''})` : '—';
  document.getElementById('revealBotLabel').textContent = label;
  document.getElementById('revealBackdrop').style.display = 'flex';
}

function closeRevealModal() {
  document.getElementById('revealBackdrop').style.display = 'none';
}

async function revealToken() {
  const token = getSessionToken();
  const password = (document.getElementById('revealPassword').value || '').trim();
  if (!revealBotId) return notifications.error('Бот не выбран');
  if (!password) return notifications.error('Введите пароль');

  const data = await apiPost('/api/reveal-bot-token', { token, bot_id: revealBotId, password });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось показать токен');
    return;
  }

  document.getElementById('revealToken').textContent = (data.data && data.data.bot_token) ? data.data.bot_token : '';
  document.getElementById('revealTokenBox').style.display = 'block';
  notifications.success('Токен показан');
}

async function copyRevealToken() {
  const token = document.getElementById('revealToken').textContent || '';
  if (!token) return;
  try {
    await navigator.clipboard.writeText(token);
    notifications.success('Токен скопирован');
  } catch (_) {
    notifications.error('Не удалось скопировать (браузер запретил)');
  }
}

function openEditModal(bot) {
  editBotId = bot?.id || '';
  document.getElementById('editName').value = bot?.bot_name || '';
  document.getElementById('editDescription').value = bot?.bot_description || '';
  document.getElementById('editAvatar').value = '';
  document.getElementById('editBackdrop').style.display = 'flex';
}

function closeEditModal() {
  document.getElementById('editBackdrop').style.display = 'none';
}

async function saveBotProfile() {
  const token = getSessionToken();
  const bot_name = (document.getElementById('editName').value || '').trim();
  const bot_description = (document.getElementById('editDescription').value || '').trim();
  const file = document.getElementById('editAvatar').files?.[0] || null;

  if (!editBotId) return notifications.error('Бот не выбран');
  if (!bot_name) return notifications.error('Укажи название');

  const data = await apiPost('/api/update-bot-profile', { token, bot_id: editBotId, bot_name, bot_description });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось сохранить профиль');
    return;
  }

  if (file) {
    if (!file.type.startsWith('image/')) {
      return notifications.error('Аватар должен быть картинкой');
    }
    if (file.size > 10 * 1024 * 1024) {
      return notifications.error('Размер файла не должен превышать 10 МБ');
    }

    const form = new FormData();
    form.append('token', token);
    form.append('bot_id', editBotId);
    form.append('avatar', file);
    const res = await fetch(API_BASE + '/api/upload_bot_avatar', { method: 'POST', body: form });
    const j = await res.json();
    if (!(j && j.status === 'ok' && j.url)) {
      notifications.error(j?.error || 'Не удалось загрузить аватар');
      return;
    }
  }

  notifications.success('Сохранено');
  closeEditModal();
  await loadBots();
}

document.addEventListener('DOMContentLoaded', async () => {
  if (!(await requireAuthOrRedirect())) return;

  const isPremium = await resolvePremiumStatus();
  if (!isPremium) {
    setPremiumLocked(true);
    return;
  }
  setPremiumLocked(false);

  document.getElementById('openCreate').addEventListener('click', openCreateModal);
  document.getElementById('closeCreate').addEventListener('click', closeCreateModal);
  document.getElementById('createBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'createBackdrop') closeCreateModal();
  });
  document.getElementById('createBtn').addEventListener('click', createBot);
  document.getElementById('copyToken').addEventListener('click', copyCreatedToken);

  document.getElementById('closeReveal').addEventListener('click', closeRevealModal);
  document.getElementById('revealBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'revealBackdrop') closeRevealModal();
  });
  document.getElementById('revealBtn').addEventListener('click', revealToken);
  document.getElementById('copyRevealToken').addEventListener('click', copyRevealToken);

  document.getElementById('closeEdit').addEventListener('click', closeEditModal);
  document.getElementById('editBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'editBackdrop') closeEditModal();
  });
  document.getElementById('saveEdit').addEventListener('click', saveBotProfile);

  document.getElementById('closeAddToGroup').addEventListener('click', closeAddToGroupModal);
  document.getElementById('addToGroupBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'addToGroupBackdrop') closeAddToGroupModal();
  });
  document.getElementById('confirmAddToGroup').addEventListener('click', confirmAddToGroup);

  // builder modal
  document.getElementById('closeBuilder').addEventListener('click', closeBuilderModal);
  document.getElementById('builderBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'builderBackdrop') closeBuilderModal();
  });
  document.getElementById('saveBuilder').addEventListener('click', saveBuilder);
  document.querySelectorAll('#builderTabs .tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('#builderTabs .tab').forEach(t => t.classList.remove('active'));
      tab.classList.add('active');
      const which = tab.dataset.tab;
      document.getElementById('panelSimple').classList.toggle('active', which === 'simple');
      document.getElementById('panelAdvanced').classList.toggle('active', which === 'advanced');
      document.getElementById('panelPython').classList.toggle('active', which === 'python');
    });
  });
  ['cfgRules','cfgWelcome','cfgModeration','cfgAutoReply'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('change', setPanelVisibility);
  });
  document.getElementById('cfgAddAutoReply').addEventListener('click', () => {
    const wrap = document.getElementById('cfgAutoReplyList');
    const current = Array.isArray(wrap._pairs) ? wrap._pairs : [];
    current.push({ key: '', value: '' });
    renderAutoReplyList(current);
  });

  document.getElementById('cfgAddCmd').addEventListener('click', () => {
    const wrap = document.getElementById('cfgCmdList');
    const current = Array.isArray(wrap._items) ? wrap._items : [];
    current.push({ cmd: '', reply: '' });
    renderCmdList(current);
  });

  document.getElementById('pyInsertTemplate').addEventListener('click', () => {
    const tpl =
`# Xipher Python Bot\n#\n# Доступно:\n# - update: dict (type, scope, scope_id, from_user_id, from_role, text, joined_username)\n# - api.send(text)\n# - api.send_dm(user_id, text)\n# - api.send_group(group_id, text)\n#\n\ndef handle(update, api):\n    t = (update.get('text') or '').strip()\n    if update.get('type') == 'member_joined':\n        name = update.get('joined_username') or 'user'\n        api.send(f\"Привет, @{name}! Добро пожаловать 👋\")\n        return\n\n    if t in ('/start', '/help'):\n        api.send('Команды: /help, /ping')\n        return\n\n    if t == '/ping':\n        api.send('pong')\n        return\n\n    # echo fallback\n    if t:\n        api.send('Я получил: ' + t)\n`;
    document.getElementById('pyCode').value = tpl;
    document.getElementById('pyEnabled').checked = true;
    notifications.success('Шаблон вставлен');
  });

  // Delete bot modal
  document.getElementById('closeDelete').addEventListener('click', closeDeleteModal);
  document.getElementById('confirmDelete').addEventListener('click', confirmDeleteBot);
  document.getElementById('deleteBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'deleteBackdrop') closeDeleteModal();
  });

  // Developers modal
  document.getElementById('closeDevelopers').addEventListener('click', closeDevelopersModal);
  document.getElementById('addDeveloperBtn').addEventListener('click', addDeveloper);
  document.getElementById('developerUsername').addEventListener('keypress', (e) => {
    if (e.key === 'Enter') addDeveloper();
  });
  document.getElementById('developersBackdrop').addEventListener('click', (e) => {
    if (e.target && e.target.id === 'developersBackdrop') closeDevelopersModal();
  });

  setupTemplatesUI();

  await loadBots();
});

let deleteBotId = '';

function openDeleteModal(bot) {
  deleteBotId = bot?.id || '';
  const label = bot ? `${bot.bot_name || 'Bot'} (@${bot.bot_username || ''})` : '—';
  document.getElementById('deleteBotLabel').textContent = label;
  document.getElementById('deleteBackdrop').style.display = 'flex';
}

function closeDeleteModal() {
  document.getElementById('deleteBackdrop').style.display = 'none';
  deleteBotId = '';
}

async function confirmDeleteBot() {
  if (!deleteBotId) return notifications.error('Бот не выбран');
  const token = getSessionToken();
  const data = await apiPost('/api/delete-bot', { token, bot_id: deleteBotId });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось удалить бота');
    return;
  }
  notifications.success('Бот удалён');
  closeDeleteModal();
  await loadBots();
}

let developersBotId = '';
let developersCache = [];

async function openDevelopersModal(bot) {
  developersBotId = bot?.id || '';
  const label = bot ? `${bot.bot_name || 'Bot'} (@${bot.bot_username || ''})` : '—';
  document.getElementById('developersBotLabel').textContent = label;
  document.getElementById('developersBackdrop').style.display = 'flex';
  await loadDevelopers();
}

function closeDevelopersModal() {
  document.getElementById('developersBackdrop').style.display = 'none';
  developersBotId = '';
  developersCache = [];
}

async function loadDevelopers() {
  if (!developersBotId) return;
  const token = getSessionToken();
  const data = await apiPost('/api/get-bot-developers', { token, bot_id: developersBotId });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось загрузить разработчиков');
    return;
  }
  developersCache = data.developers || [];
  renderDevelopers();
}

function renderDevelopers() {
  const list = document.getElementById('developersList');
  list.innerHTML = '';
  if (!developersCache.length) {
    list.innerHTML = '<div class="muted">Нет разработчиков</div>';
    return;
  }
  for (const dev of developersCache) {
    const row = document.createElement('div');
    row.style.display = 'flex';
    row.style.justifyContent = 'space-between';
    row.style.alignItems = 'center';
    row.style.padding = '8px 0';
    row.style.borderBottom = '1px solid var(--border-color)';
    row.innerHTML = `
      <div>
        <div style="font-weight: 600;">@${escapeHtml(dev.developer_username || '')}</div>
        <div class="muted" style="font-size: 0.85rem;">Добавлен ${escapeHtml(dev.added_at || '')}</div>
      </div>
      <button class="btn" style="color: var(--error-color, #ef4444);" data-dev-id="${escapeHtml(dev.developer_user_id)}">Удалить</button>
    `;
    const removeBtn = row.querySelector('button');
    removeBtn.addEventListener('click', () => removeDeveloper(dev.developer_user_id));
    list.appendChild(row);
  }
}

async function addDeveloper() {
  if (!developersBotId) return;
  const username = (document.getElementById('developerUsername').value || '').trim();
  if (!username) return notifications.error('Укажи username');
  const token = getSessionToken();
  const data = await apiPost('/api/add-bot-developer', { token, bot_id: developersBotId, developer_username: username });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось добавить разработчика');
    return;
  }
  notifications.success('Разработчик добавлен');
  document.getElementById('developerUsername').value = '';
  await loadDevelopers();
}

async function removeDeveloper(devUserId) {
  if (!developersBotId || !devUserId) return;
  if (!confirm('Удалить разработчика?')) return;
  const token = getSessionToken();
  const data = await apiPost('/api/remove-bot-developer', { token, bot_id: developersBotId, developer_user_id: devUserId });
  if (!data || !data.success) {
    notifications.error(data?.message || 'Не удалось удалить разработчика');
    return;
  }
  notifications.success('Разработчик удалён');
  await loadDevelopers();
}
