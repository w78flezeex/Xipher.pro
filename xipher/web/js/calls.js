// WebRTC звонки функционал - полностью переделанная версия с поддержкой групповых звонков

// SVG иконки в стиле Telegram
const CALL_ICONS = {
    mic: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z"/><path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z"/></svg>',
    micOff: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 11h-1.7c0 .74-.16 1.43-.43 2.05l1.23 1.23c.56-.98.9-2.09.9-3.28zm-4.02.17c0-.06.02-.11.02-.17V5c0-1.66-1.34-3-3-3S9 3.34 9 5v.18l5.98 5.99zM4.27 3L3 4.27l6.01 6.01V11c0 1.66 1.33 3 2.99 3 .22 0 .44-.03.65-.08l1.66 1.66c-.71.33-1.5.52-2.31.52-2.76 0-5.3-2.1-5.3-5.1H5c0 3.41 2.72 6.23 6 6.72V21h2v-3.28c.91-.13 1.77-.45 2.54-.9L19.73 21 21 19.73 4.27 3z"/></svg>',
    video: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M17 10.5V7c0-.55-.45-1-1-1H4c-.55 0-1 .45-1 1v10c0 .55.45 1 1 1h12c.55 0 1-.45 1-1v-3.5l4 4v-11l-4 4z"/></svg>',
    videoOff: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M21 6.5l-4 4V7c0-.55-.45-1-1-1H9.82L21 17.18V6.5zM3.27 2L2 3.27 4.73 6H4c-.55 0-1 .45-1 1v10c0 .55.45 1 1 1h12c.21 0 .39-.08.54-.18L19.73 21 21 19.73 3.27 2z"/></svg>',
    screen: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h5v2h8v-2h5c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12z"/></svg>',
    screenOff: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h5v2h8v-2h5c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12z"/><path d="M3.41 2.13L2 3.54l18.46 18.46 1.41-1.41L3.41 2.13z"/></svg>',
    speaker: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></svg>',
    speakerOff: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M16.5 12c0-1.77-1.02-3.29-2.5-4.03v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51C20.63 14.91 21 13.5 21 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06c1.38-.31 2.63-.95 3.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></svg>',
    phone: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M20.01 15.38c-1.23 0-2.42-.2-3.53-.56-.35-.12-.74-.03-1.01.24l-1.57 1.97c-2.83-1.35-5.48-3.9-6.89-6.83l1.95-1.66c.27-.28.35-.67.24-1.02-.37-1.11-.56-2.3-.56-3.53 0-.54-.45-.99-.99-.99H4.19C3.65 3 3 3.24 3 3.99 3 13.28 10.73 21 20.01 21c.71 0 .99-.63.99-1.18v-3.45c0-.54-.45-.99-.99-.99z"/></svg>',
    phoneOff: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 9c-1.6 0-3.15.25-4.6.72v3.1c0 .39-.23.74-.56.9-.98.49-1.87 1.12-2.66 1.85-.18.18-.43.28-.7.28-.28 0-.53-.11-.71-.29L.29 13.08c-.18-.17-.29-.42-.29-.7 0-.28.11-.53.29-.71C3.34 8.78 7.46 7 12 7s8.66 1.78 11.71 4.67c.18.18.29.43.29.71 0 .28-.11.53-.29.71l-2.48 2.48c-.18.18-.43.29-.71.29-.27 0-.52-.11-.7-.28-.79-.74-1.69-1.36-2.67-1.85-.33-.16-.56-.5-.56-.9v-3.1C15.15 9.25 13.6 9 12 9z"/></svg>',
    hangup: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 9c-1.6 0-3.15.25-4.6.72v3.1c0 .39-.23.74-.56.9-.98.49-1.87 1.12-2.66 1.85-.18.18-.43.28-.7.28-.28 0-.53-.11-.71-.29L.29 13.08c-.18-.17-.29-.42-.29-.7 0-.28.11-.53.29-.71C3.34 8.78 7.46 7 12 7s8.66 1.78 11.71 4.67c.18.18.29.43.29.71 0 .28-.11.53-.29.71l-2.48 2.48c-.18.18-.43.29-.71.29-.27 0-.52-.11-.7-.28-.79-.74-1.69-1.36-2.67-1.85-.33-.16-.56-.5-.56-.9v-3.1C15.15 9.25 13.6 9 12 9z"/></svg>',
    settings: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>',
    minimize: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 13H5v-2h14v2z"/></svg>',
    maximize: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/></svg>',
    close: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/></svg>',
    noise: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M7.76 16.24C6.67 15.16 6 13.66 6 12s.67-3.16 1.76-4.24l1.42 1.42C8.45 9.9 8 10.9 8 12c0 1.1.45 2.1 1.17 2.83l-1.41 1.41zm8.48 0C17.33 15.16 18 13.66 18 12s-.67-3.16-1.76-4.24l-1.42 1.42C15.55 9.9 16 10.9 16 12c0 1.1-.45 2.1-1.17 2.83l1.41 1.41zM12 10c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm-5.66 7.66C4.79 16.11 4 14.15 4 12s.79-4.11 2.34-5.66l1.42 1.42C6.59 8.93 6 10.4 6 12s.59 3.07 1.76 4.24l-1.42 1.42zm11.32 0l-1.42-1.42C17.41 15.07 18 13.6 18 12s-.59-3.07-1.76-4.24l1.42-1.42C19.21 7.89 20 9.85 20 12s-.79 4.11-2.34 5.66z"/></svg>',
    lock: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>',
    headphones: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 1c-4.97 0-9 4.03-9 9v7c0 1.66 1.34 3 3 3h3v-8H5v-2c0-3.87 3.13-7 7-7s7 3.13 7 7v2h-4v8h3c1.66 0 3-1.34 3-3v-7c0-4.97-4.03-9-9-9z"/></svg>',
    premium: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 17.27L18.18 21l-1.64-7.03L22 9.24l-7.19-.61L12 2 9.19 8.63 2 9.24l5.46 4.73L5.82 21z"/></svg>'
};

// Хелпер для вставки иконки
function callIcon(name, className = '') {
    const icon = CALL_ICONS[name] || '';
    return `<span class="call-icon ${className}">${icon}</span>`;
}

// Для обратной совместимости с одиночными звонками
let peerConnection = null;
// Для групповых звонков - Map<userId, RTCPeerConnection>
let peerConnections = new Map();
// Map<userId, MediaStream> для хранения удаленных потоков
let remoteStreams = new Map();
// Set участников звонка
let callParticipants = new Set();
let isGroupCall = false; // true если это групповой звонок
let pendingGroupSfuConfig = null;

let localStream = null;
let rawLocalStream = null;
let remoteStream = null; // Для обратной совместимости
let isCallActive = false;
let callStartTime = null;
let callTimer = null;
let currentCallType = null; // 'audio' or 'video'
let screenStream = null;
let availableDevices = {
    audioInputs: [],
    audioOutputs: [],
    videoInputs: []
};
let selectedAudioInputId = null;
let selectedAudioOutputId = null;
let isCallMinimized = false;
let callCheckInterval = null; // Интервал для проверки входящих звонков
let callResponseCheckInterval = null; // Интервал для проверки ответа на звонок
let callSignalingInterval = null; // Интервал для проверки offer/answer/ICE кандидатов
let callAnswerPollInterval = null;
let callIcePollInterval = null;
let lastCallIceCheck = 0;
let currentCallId = null; // ID текущего звонка для предотвращения дублирования
let isCaller = false; // true если мы инициатор звонка, false если принимаем
let lastSignalingCheck = Date.now(); // Время последней проверки сигналинга
let callRingtone = null; // Аудио элемент для звука звонка
let callRingtoneInterval = null; // Интервал для повторения звука звонка
let callControlsBar = null; // Плавающая панель управления звонком
let callControlsOffsetListenerAttached = false;
let callInlinePrompt = null; // Встроенная панель принятия/отклонения
let callTypeModalReady = false;
let callEndSent = false; // Гард от повторных call_end
const CALL_RECOVERY_CONFIG = {
    graceMs: 180000,       // 3 минуты на восстановление
    retryMs: 4000,         // Быстрее retry
    maxAttempts: 10        // Больше попыток
};
const GROUP_RECOVERY_GRACE_MS = 45000;  // 45 сек для групповых
let callRecoveryState = {
    active: false,
    startedAt: 0,
    attempts: 0,
    timer: null,
    inFlight: false,
    noticeSent: false
};
let pendingIceCandidates = []; // Буфер ICE до установки remoteDescription
/** @type {AudioContext|null} */ // FIX: Явное хранилище аудио-контекста для разблокировки автоплея
let remoteAudioContext = null;
let remoteAudioSource = null;
let remoteAudioStream = null;
let remoteAudioGain = null;
let remoteAudioDestination = null;
let remoteAudioProcessedStream = null;
let localAudioContext = null;
let localAudioNodes = null;
let localNoiseGateInterval = null;
let localNoiseGateState = {
    open: false,
    lastOpenAt: 0
};
let micVolumeLevel = 1.0;
let headphoneVolumeLevel = 1.0;
let rnnoiseInstancePromise = null;
let rnnoiseInstanceUrl = null;
let rnnoiseState = null;
let rnnoiseNode = null;
const MAX_GROUP_ACTIVE_SENDERS = 6; // soft cap on upstream senders in group calls
let groupActiveSenders = new Set();
// FIX: Сторож тишины
let audioWatchdogInterval = null;
let audioWatchdogState = {
    lastBytes: 0,
    lastLevel: 0,
    silentSince: null,
    restartCount: 0
};
// FIX: Флаг уведомления о шаринге экрана удаленным пользователем
let remoteScreenShareNotified = false;
let remoteScreenShareActive = false;
let remoteScreenShareTrackId = null;
let permissionMonitorInterval = null;

// Состояние медиа удаленного пользователя
let remoteMediaState = {
    micEnabled: true,
    videoEnabled: true,
    speakerEnabled: true
};

const DEFAULT_CALL_QUALITY_CONFIG = {
    storageKeys: {
        camera: 'xipher_call_quality_camera',
        screen: 'xipher_call_quality_screen'
    },
    defaults: {
        camera: { resolution: 1080, fps: 30 },
        screen: { resolution: 1080, fps: 30 }
    },
    resolutionOptions: [360, 540, 720, 1080],
    fpsOptions: [15, 30, 60],
    freeCap: { resolution: 1080, fps: 30 },
    premiumCap: { resolution: 1080, fps: 60 },
    bitrateBpp: { camera: 0.08, screen: 0.10 },
    bitrateCaps: {
        camera: { free: 3_500_000, premium: 8_000_000 },
        screen: { free: 2_500_000, premium: 10_000_000 }
    },
    bitrateFloor: { camera: 500_000, screen: 750_000 }
};

if (typeof window !== 'undefined') {
    window.XIPHER_CALL_QUALITY = window.XIPHER_CALL_QUALITY || DEFAULT_CALL_QUALITY_CONFIG;
}

const CALLS_QUALITY_CONFIG = (typeof window !== 'undefined' && window.XIPHER_CALL_QUALITY)
    ? window.XIPHER_CALL_QUALITY
    : DEFAULT_CALL_QUALITY_CONFIG;
const CALLS_QUALITY_STORAGE_KEYS = CALLS_QUALITY_CONFIG.storageKeys || DEFAULT_CALL_QUALITY_CONFIG.storageKeys;
const CALLS_QUALITY_DEFAULTS = CALLS_QUALITY_CONFIG.defaults || DEFAULT_CALL_QUALITY_CONFIG.defaults;
const CALLS_QUALITY_RESOLUTION_OPTIONS = CALLS_QUALITY_CONFIG.resolutionOptions || DEFAULT_CALL_QUALITY_CONFIG.resolutionOptions;
const CALLS_QUALITY_FPS_OPTIONS = CALLS_QUALITY_CONFIG.fpsOptions || DEFAULT_CALL_QUALITY_CONFIG.fpsOptions;
const CALLS_QUALITY_FREE_CAP = CALLS_QUALITY_CONFIG.freeCap || DEFAULT_CALL_QUALITY_CONFIG.freeCap;
const CALLS_QUALITY_PREMIUM_CAP = CALLS_QUALITY_CONFIG.premiumCap || DEFAULT_CALL_QUALITY_CONFIG.premiumCap;
const CALLS_QUALITY_BITRATE_BPP = CALLS_QUALITY_CONFIG.bitrateBpp || DEFAULT_CALL_QUALITY_CONFIG.bitrateBpp;
const CALLS_QUALITY_BITRATE_CAPS = CALLS_QUALITY_CONFIG.bitrateCaps || DEFAULT_CALL_QUALITY_CONFIG.bitrateCaps;
const CALLS_QUALITY_BITRATE_FLOOR = CALLS_QUALITY_CONFIG.bitrateFloor || DEFAULT_CALL_QUALITY_CONFIG.bitrateFloor;

const DEFAULT_CALL_CAMERA_QUALITY = CALLS_QUALITY_DEFAULTS.camera || { resolution: 720, fps: 15 };
const DEFAULT_CALL_CAMERA_RESOLUTION = Number.isFinite(DEFAULT_CALL_CAMERA_QUALITY.resolution)
    ? DEFAULT_CALL_CAMERA_QUALITY.resolution
    : 720;
const DEFAULT_CALL_CAMERA_FPS = Number.isFinite(DEFAULT_CALL_CAMERA_QUALITY.fps)
    ? DEFAULT_CALL_CAMERA_QUALITY.fps
    : 15;
const DEFAULT_CALL_CAMERA_WIDTH = Math.round(DEFAULT_CALL_CAMERA_RESOLUTION * 16 / 9);
const STRICT_MEDIA_CONSTRAINTS = {
    audio: {
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true
    },
    video: {
        width: { ideal: DEFAULT_CALL_CAMERA_WIDTH },
        height: { ideal: DEFAULT_CALL_CAMERA_RESOLUTION },
        frameRate: { ideal: DEFAULT_CALL_CAMERA_FPS }
    }
};
const PERMISSIVE_MEDIA_CONSTRAINTS = { audio: true, video: true };
const AUDIO_VOLUME_MAX = 1.5;
const AUDIO_VOLUME_MIN = 0;
const RNNOISE_DEFAULT_URL = 'https://cdn.jsdelivr.net/npm/@shiguredo/rnnoise-wasm@2025.2.0-canary.0/dist/rnnoise.min.js';
let lastIncomingOffer = null;
let lastIncomingOfferAt = 0;

// Базовые ICE (STUN + опциональный TURN). TURN берем динамически через /api/turn-credentials.
// Множественные STUN серверы для надёжности. TURN берем динамически.
const BASE_ICE_SERVERS = [
    { urls: 'stun:turn.xipher.pro:3478' },
    { urls: 'stun:stun.l.google.com:19302' },
    { urls: 'stun:stun1.l.google.com:19302' },
    { urls: 'stun:stun2.l.google.com:19302' }
];
const TURN_TEMPLATE = {
    urls: [
        'turn:turn.xipher.pro:3478?transport=udp',
        'turn:turn.xipher.pro:3478?transport=tcp',
        'turns:turn.xipher.pro:5349?transport=tcp'
    ],
    username: 'REPLACE_ME',
    credential: 'REPLACE_ME'
};
let cachedIceServers = null;
let turnConfigPromise = null;
let turnConfigExpiresAt = 0;
const TURN_REFRESH_SKEW_MS = 5 * 60 * 1000;

// Буфер для ICE кандидатов с таймстемпами
let pendingIceCandidatesWithTs = [];
const ICE_BUFFER_TIMEOUT_MS = 30000; // 30 сек максимум ждём remoteDescription

function normalizeIceServers(rawServers) {
    if (!Array.isArray(rawServers)) return [];
    const normalized = [];
    rawServers.forEach(server => {
        if (!server) return;
        const urlsRaw = server.urls || server.url;
        const urls = Array.isArray(urlsRaw)
            ? urlsRaw
            : typeof urlsRaw === 'string'
                ? urlsRaw.split(',').map(s => s.trim()).filter(Boolean)
                : [];
        if (!urls.length) return;
        const entry = { urls };
        if (server.username) entry.username = server.username;
        if (server.credential) entry.credential = server.credential;
        normalized.push(entry);
    });
    return normalized;
}

function looksLikeBase64(value) {
    if (typeof value !== 'string') return false;
    if (!value || value.length % 4 !== 0) return false;
    return /^[A-Za-z0-9+/=]+$/.test(value);
}

function safeBase64Decode(value) {
    if (typeof value !== 'string') return null;
    try {
        return decodeURIComponent(escape(atob(value)));
    } catch (err) {
        console.warn('[Call] base64 decode failed:', err);
        return null;
    }
}

function safeJsonParse(value) {
    if (typeof value !== 'string') return null;
    try {
        return JSON.parse(value);
    } catch (err) {
        return null;
    }
}

function sanitizeJsonString(value) {
    if (typeof value !== 'string') return value;
    let cleaned = value.replace(/\r/g, '\\r').replace(/\n/g, '\\n');
    cleaned = cleaned.replace(/\\c/g, '\\r');
    cleaned = cleaned.replace(/\\(?![\"\\/bfnrtu])/g, '\\\\');
    return cleaned;
}

function looksLikeSdp(value) {
    return typeof value === 'string'
        && (value.startsWith('v=') || value.includes('\nv=') || value.includes('\r\nv='));
}

function decodeSignalingPayload(raw, encodingHint) {
    if (raw == null) return null;
    if (typeof raw !== 'string') return raw;
    let value = raw;
    if (encodingHint === 'b64' || looksLikeBase64(value)) {
        const decoded = safeBase64Decode(value);
        if (decoded) value = decoded;
    }
    let parsed = safeJsonParse(value);
    if (parsed != null) return parsed;
    const sanitized = sanitizeJsonString(value);
    parsed = safeJsonParse(sanitized);
    return parsed != null ? parsed : value;
}
function normalizeCallOfferPayload(raw) {
    if (!raw) return null;
    if (typeof raw === 'string') {
        const cleaned = sanitizeJsonString(raw);
        const parsed = safeJsonParse(cleaned);
        if (parsed) return parsed;
        if (looksLikeSdp(raw)) {
            return { type: 'offer', sdp: raw };
        }
        return null;
    }
    if (typeof raw === 'object') {
        return raw;
    }
    return null;
}

function isCallQualityPremiumEnabled() {
    if (typeof isPremiumActive === 'function') {
        return isPremiumActive();
    }
    if (typeof currentUser !== 'undefined' && currentUser && typeof currentUser.is_premium === 'boolean') {
        return currentUser.is_premium;
    }
    try {
        return localStorage.getItem('xipher_premium_active') === 'true';
    } catch (_) {
        return false;
    }
}

function normalizeCallQualityValueForCall(value, kind, isPremium) {
    const fallback = CALLS_QUALITY_DEFAULTS[kind] || { resolution: 720, fps: 15 };
    let resolution = fallback.resolution;
    let fps = fallback.fps;
    if (value && typeof value === 'object') {
        const parsedResolution = parseInt(value.resolution, 10);
        const parsedFps = parseInt(value.fps, 10);
        if (CALLS_QUALITY_RESOLUTION_OPTIONS.includes(parsedResolution)) {
            resolution = parsedResolution;
        }
        if (CALLS_QUALITY_FPS_OPTIONS.includes(parsedFps)) {
            fps = parsedFps;
        }
    }
    const cap = isPremium ? CALLS_QUALITY_PREMIUM_CAP : CALLS_QUALITY_FREE_CAP;
    resolution = Math.min(resolution, cap.resolution);
    fps = Math.min(fps, cap.fps);
    return { resolution, fps };
}

function readCallQualitySettingForCall(kind, isPremium) {
    const key = CALLS_QUALITY_STORAGE_KEYS[kind];
    const fallback = CALLS_QUALITY_DEFAULTS[kind] || { resolution: 720, fps: 15 };
    if (!key) return normalizeCallQualityValueForCall(fallback, kind, isPremium);
    try {
        const raw = localStorage.getItem(key);
        if (!raw) return normalizeCallQualityValueForCall(fallback, kind, isPremium);
        const parsed = JSON.parse(raw);
        return normalizeCallQualityValueForCall(parsed, kind, isPremium);
    } catch (_) {
        return normalizeCallQualityValueForCall(fallback, kind, isPremium);
    }
}

function writeCallQualitySettingForCall(kind, value) {
    const key = CALLS_QUALITY_STORAGE_KEYS[kind];
    if (!key) return;
    try {
        localStorage.setItem(key, JSON.stringify(value));
    } catch (_) {}
}

function getCallQualitySettingForCall(kind) {
    const isPremium = isCallQualityPremiumEnabled();
    const normalized = readCallQualitySettingForCall(kind, isPremium);
    writeCallQualitySettingForCall(kind, normalized);
    return normalized;
}

function getVideoDimensionsForCall(resolution) {
    const height = Number.isFinite(resolution) ? resolution : 720;
    const width = Math.round(height * 16 / 9);
    return { width, height };
}

function buildCameraVideoConstraintsForCall() {
    const quality = getCallQualitySettingForCall('camera');
    const { width, height } = getVideoDimensionsForCall(quality.resolution);
    return {
        width: { ideal: width },
        height: { ideal: height },
        frameRate: { ideal: quality.fps, max: quality.fps }
    };
}

function buildScreenShareConstraintsForCall() {
    const quality = getCallQualitySettingForCall('screen');
    const { width, height } = getVideoDimensionsForCall(quality.resolution);
    return {
        video: {
            width: { max: width },
            height: { max: height },
            frameRate: { max: quality.fps },
            cursor: 'always',
            displaySurface: 'monitor'
        },
        audio: {
            echoCancellation: true,
            noiseSuppression: true,
            autoGainControl: true
        }
    };
}

function estimateVideoBitrateForCall(kind, quality, isPremium) {
    const safeQuality = normalizeCallQualityValueForCall(quality, kind, isPremium);
    const { width, height } = getVideoDimensionsForCall(safeQuality.resolution);
    const fps = Number.isFinite(safeQuality.fps) ? safeQuality.fps : 15;
    const bpp = (CALLS_QUALITY_BITRATE_BPP && CALLS_QUALITY_BITRATE_BPP[kind])
        ? CALLS_QUALITY_BITRATE_BPP[kind]
        : (kind === 'screen' ? 0.08 : 0.06);
    let bitrate = Math.round(width * height * fps * bpp);
    const floor = CALLS_QUALITY_BITRATE_FLOOR && CALLS_QUALITY_BITRATE_FLOOR[kind];
    const caps = CALLS_QUALITY_BITRATE_CAPS && CALLS_QUALITY_BITRATE_CAPS[kind];
    const cap = caps ? (isPremium ? caps.premium : caps.free) : null;
    if (Number.isFinite(floor)) {
        bitrate = Math.max(bitrate, floor);
    }
    if (Number.isFinite(cap)) {
        bitrate = Math.min(bitrate, cap);
    }
    return bitrate;
}

async function applyVideoEncodingForCall(sender, kind, quality) {
    if (!sender || typeof sender.getParameters !== 'function' || typeof sender.setParameters !== 'function') return;
    try {
        const isPremium = isCallQualityPremiumEnabled();
        const normalized = normalizeCallQualityValueForCall(quality, kind, isPremium);
        const maxBitrate = estimateVideoBitrateForCall(kind, normalized, isPremium);
        const params = sender.getParameters();
        if (!params.encodings || !params.encodings.length) {
            params.encodings = [{}];
        }
        if (Number.isFinite(maxBitrate)) {
            params.encodings[0].maxBitrate = maxBitrate;
        }
        if (Number.isFinite(normalized.fps)) {
            params.encodings[0].maxFramerate = normalized.fps;
        }
        if (kind === 'screen') {
            params.degradationPreference = 'maintain-resolution';
        } else if (!params.degradationPreference) {
            params.degradationPreference = 'balanced';
        }
        await sender.setParameters(params);
    } catch (err) {
        console.warn('[Call] Failed to apply video encoding params:', err);
    }
}

async function applyCameraEncodingForCall(sender, quality) {
    const resolved = quality || getCallQualitySettingForCall('camera');
    await applyVideoEncodingForCall(sender, 'camera', resolved);
}

function updateInlineCallQualityControls() {
    const isPremium = isCallQualityPremiumEnabled();
    const cameraResolution = document.getElementById('callCameraQualitySelectInline');
    const cameraFps = document.getElementById('callCameraFpsSelectInline');
    const screenResolution = document.getElementById('callScreenQualitySelectInline');
    const screenFps = document.getElementById('callScreenFpsSelectInline');
    const premiumNote = document.getElementById('callQualityPremiumNoteInline');

    const togglePremiumOptions = (select) => {
        if (!select) return;
        select.querySelectorAll('[data-premium]').forEach(option => {
            option.disabled = !isPremium;
        });
    };

    togglePremiumOptions(cameraResolution);
    togglePremiumOptions(cameraFps);
    togglePremiumOptions(screenResolution);
    togglePremiumOptions(screenFps);

    const cameraValue = getCallQualitySettingForCall('camera');
    const screenValue = getCallQualitySettingForCall('screen');
    if (cameraResolution) cameraResolution.value = String(cameraValue.resolution);
    if (cameraFps) cameraFps.value = String(cameraValue.fps);
    if (screenResolution) screenResolution.value = String(screenValue.resolution);
    if (screenFps) screenFps.value = String(screenValue.fps);

    if (premiumNote) {
        premiumNote.style.display = isPremium ? 'none' : 'block';
    }
}

function bindInlineCallQualityControls() {
    const cameraResolution = document.getElementById('callCameraQualitySelectInline');
    const cameraFps = document.getElementById('callCameraFpsSelectInline');
    const screenResolution = document.getElementById('callScreenQualitySelectInline');
    const screenFps = document.getElementById('callScreenFpsSelectInline');

    const handleChange = (kind, resolutionEl, fpsEl) => {
        if (!resolutionEl || !fpsEl) return;
        const isPremium = isCallQualityPremiumEnabled();
        const next = {
            resolution: parseInt(resolutionEl.value, 10),
            fps: parseInt(fpsEl.value, 10)
        };
        const normalized = normalizeCallQualityValueForCall(next, kind, isPremium);
        if (resolutionEl.value !== String(normalized.resolution)) {
            resolutionEl.value = String(normalized.resolution);
        }
        if (fpsEl.value !== String(normalized.fps)) {
            fpsEl.value = String(normalized.fps);
        }
        writeCallQualitySettingForCall(kind, normalized);
    };

    cameraResolution?.addEventListener('change', () => handleChange('camera', cameraResolution, cameraFps));
    cameraFps?.addEventListener('change', () => handleChange('camera', cameraResolution, cameraFps));
    screenResolution?.addEventListener('change', () => handleChange('screen', screenResolution, screenFps));
    screenFps?.addEventListener('change', () => handleChange('screen', screenResolution, screenFps));
}

const SFU_TRANSACTION_TIMEOUT_MS = 10000;
const SFU_KEEPALIVE_INTERVAL_MS = 25000;
let sfuState = {
    active: false,
    janusUrl: '',
    roomId: null,
    roomReady: false,
    sessionId: null,
    ws: null,
    keepaliveInterval: null,
    transactions: new Map(),
    handles: new Map(),
    subscribers: new Map(),
    publisher: null,
    publisherDisplay: '',
    publisherJoinPromise: null,
    publisherJoinResolve: null
};
const sfuParticipantLabels = new Map();
const sfuFeedMeta = new Map();
let sfuScreenShareCount = 0;

function buildSfuParticipantId(feedId) {
    return `sfu-${feedId}`;
}

function parseSfuDisplay(display) {
    if (!display) return { name: 'Участник', userId: '' };
    try {
        const parsed = JSON.parse(display);
        return {
            name: parsed.name || parsed.username || parsed.user || display,
            userId: parsed.user_id || parsed.userId || ''
        };
    } catch (_) {
        return { name: display, userId: '' };
    }
}

function getLocalDisplayPayload() {
    const userId = localStorage.getItem('xipher_user_id') || '';
    const fallback = (currentUser && (currentUser.username || currentUser.name)) || userId || 'Участник';
    return JSON.stringify({ user_id: userId, name: fallback });
}

function clearSfuTransactions() {
    sfuState.transactions.forEach(entry => {
        clearTimeout(entry.timeoutId);
    });
    sfuState.transactions.clear();
}

function resetSfuState() {
    sfuState.active = false;
    sfuState.janusUrl = '';
    sfuState.roomId = null;
    sfuState.roomReady = false;
    sfuState.sessionId = null;
    sfuState.ws = null;
    sfuState.publisher = null;
    sfuState.publisherDisplay = '';
    sfuState.publisherJoinPromise = null;
    sfuState.publisherJoinResolve = null;
    if (sfuState.keepaliveInterval) {
        clearInterval(sfuState.keepaliveInterval);
        sfuState.keepaliveInterval = null;
    }
    clearSfuTransactions();
    sfuState.handles.clear();
    sfuState.subscribers.clear();
    sfuParticipantLabels.clear();
    sfuFeedMeta.clear();
    sfuScreenShareCount = 0;
}

async function fetchSfuRoomConfig(groupId) {
    const token = localStorage.getItem('xipher_token');
    if (!token || !groupId) return null;
    try {
        const response = await fetch(API_BASE + '/api/sfu-room', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ token, group_id: groupId })
        });
        if (!response.ok) return null;
        const data = await response.json();
        if (!data || !data.success || !data.enabled) return null;
        const janusUrl = data.janus_url || '';
        const roomId = parseInt(data.room_id, 10);
        if (!janusUrl || !Number.isFinite(roomId)) return null;
        return {
            janusUrl,
            roomId,
            roomReady: data.room_ready !== false && data.room_ready !== 'false'
        };
    } catch (err) {
        console.warn('[SFU] Failed to fetch room config:', err);
        return null;
    }
}

function createSfuTransaction() {
    return `sfu_${Date.now()}_${Math.random().toString(16).slice(2)}`;
}

function sfuSend(message) {
    if (!sfuState.ws || sfuState.ws.readyState !== WebSocket.OPEN) {
        return Promise.reject(new Error('SFU socket not ready'));
    }
    const transaction = createSfuTransaction();
    message.transaction = transaction;
    return new Promise((resolve, reject) => {
        const timeoutId = setTimeout(() => {
            sfuState.transactions.delete(transaction);
            reject(new Error('SFU transaction timeout'));
        }, SFU_TRANSACTION_TIMEOUT_MS);
        sfuState.transactions.set(transaction, { resolve, reject, timeoutId });
        sfuState.ws.send(JSON.stringify(message));
    });
}

function handleSfuMessage(raw) {
    let message;
    try {
        message = JSON.parse(raw);
    } catch (err) {
        console.warn('[SFU] Bad message:', err);
        return;
    }
    if (message.transaction && sfuState.transactions.has(message.transaction)) {
        const entry = sfuState.transactions.get(message.transaction);
        clearTimeout(entry.timeoutId);
        sfuState.transactions.delete(message.transaction);
        entry.resolve(message);
    }

    if (message.janus === 'event') {
        handleSfuEvent(message);
    } else if (message.janus === 'trickle') {
        handleSfuTrickle(message);
    } else if (message.janus === 'hangup') {
        handleSfuHangup(message);
    }
}

function handleSfuEvent(message) {
    const handleId = message.sender;
    if (!handleId) return;
    const handle = sfuState.handles.get(handleId);
    if (!handle) return;
    if (handle.type === 'publisher') {
        handleSfuPublisherEvent(handle, message).catch(err => {
            console.warn('[SFU] Publisher event error:', err);
        });
    } else if (handle.type === 'subscriber') {
        handleSfuSubscriberEvent(handle, message).catch(err => {
            console.warn('[SFU] Subscriber event error:', err);
        });
    }
}

function handleSfuTrickle(message) {
    const handleId = message.sender;
    const candidate = message.candidate;
    if (!handleId || !candidate) return;
    const handle = sfuState.handles.get(handleId);
    if (!handle) return;
    if (candidate.completed) return;
    if (!handle.pc) {
        handle.pendingCandidates = handle.pendingCandidates || [];
        handle.pendingCandidates.push(candidate);
        return;
    }
    if (handle.pc.remoteDescription) {
        handle.pc.addIceCandidate(candidate).catch(err => {
            console.warn('[SFU] Failed to add ICE:', err);
        });
    } else {
        handle.pendingCandidates = handle.pendingCandidates || [];
        handle.pendingCandidates.push(candidate);
    }
}

function handleSfuHangup(message) {
    const handleId = message.sender;
    if (!handleId) return;
    const handle = sfuState.handles.get(handleId);
    if (!handle) return;
    if (handle.type === 'subscriber' && handle.feedId != null) {
        detachSfuSubscriber(handle.feedId);
    }
}

function ensureSubscriberPeerConnection(handle) {
    if (handle.pc) return handle.pc;
    const pc = new RTCPeerConnection({ iceServers: buildIceServers() });
    handle.pc = pc;
    pc.onicecandidate = (event) => {
        if (!event.candidate) {
            sfuSend({
                janus: 'trickle',
                session_id: sfuState.sessionId,
                handle_id: handle.handleId,
                candidate: { completed: true }
            }).catch(() => {});
            return;
        }
        sfuSend({
            janus: 'trickle',
            session_id: sfuState.sessionId,
            handle_id: handle.handleId,
            candidate: event.candidate
        }).catch(() => {});
    };
    pc.ontrack = (event) => {
        const stream = (event.streams && event.streams[0])
            ? event.streams[0]
            : new MediaStream([event.track]);
        const feedId = handle.feedId;
        const participantId = buildSfuParticipantId(feedId);
        const meta = sfuFeedMeta.get(feedId) || { name: 'Участник' };
        sfuParticipantLabels.set(participantId, meta.name || 'Участник');
        remoteStreams.set(participantId, stream);
        addParticipantVideo(participantId, stream);
        if (event.track.kind === 'video' && isScreenTrack(event.track)) {
            sfuScreenShareCount += 1;
            remoteScreenShareActive = true;
            event.track.onended = () => {
                sfuScreenShareCount = Math.max(0, sfuScreenShareCount - 1);
                remoteScreenShareActive = sfuScreenShareCount > 0;
            };
        }
    };
    return pc;
}

async function handleSfuSubscriberEvent(handle, message) {
    const data = message.plugindata && message.plugindata.data ? message.plugindata.data : {};
    if (data.leaving || data.unpublished) {
        const feedId = data.leaving || data.unpublished;
        if (feedId && feedId !== 'ok') {
            detachSfuSubscriber(feedId);
        }
        return;
    }
    if (message.jsep) {
        const pc = ensureSubscriberPeerConnection(handle);
        const signalingState = pc.signalingState;
        console.log('[SFU] Subscriber signaling state:', signalingState);
        
        if (signalingState !== 'stable') {
            console.warn('[SFU] Cannot process JSEP in state:', signalingState);
            return;
        }
        
        await pc.setRemoteDescription(message.jsep);
        
        if (pc.signalingState !== 'have-remote-offer') {
            console.warn('[SFU] Unexpected state after setRemoteDescription:', pc.signalingState);
            return;
        }
        
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        await sfuSend({
            janus: 'message',
            session_id: sfuState.sessionId,
            handle_id: handle.handleId,
            body: { request: 'start', room: sfuState.roomId },
            jsep: answer
        });
        if (handle.pendingCandidates && handle.pendingCandidates.length) {
            for (const cand of handle.pendingCandidates) {
                try {
                    await pc.addIceCandidate(cand);
                } catch (err) {
                    console.warn('[SFU] Failed to add buffered ICE:', err);
                }
            }
            handle.pendingCandidates = [];
        }
    }
}

async function handleSfuPublisherEvent(handle, message) {
    const data = message.plugindata && message.plugindata.data ? message.plugindata.data : {};
    if (message.jsep && handle.pc) {
        await handle.pc.setRemoteDescription(message.jsep);
    }
    if (data.videoroom === 'joined') {
        if (sfuState.publisherJoinResolve) {
            sfuState.publisherJoinResolve();
            sfuState.publisherJoinResolve = null;
        }
        if (Array.isArray(data.publishers)) {
            for (const publisher of data.publishers) {
                await attachSfuSubscriber(publisher);
            }
        }
    } else if (data.videoroom === 'event' && Array.isArray(data.publishers)) {
        for (const publisher of data.publishers) {
            await attachSfuSubscriber(publisher);
        }
    }
    if (data.leaving || data.unpublished) {
        const feedId = data.leaving || data.unpublished;
        if (feedId && feedId !== 'ok') {
            detachSfuSubscriber(feedId);
        }
    }
}

async function attachSfuSubscriber(publisher) {
    const feedId = publisher.id;
    if (feedId == null || sfuState.subscribers.has(feedId)) return;
    const display = publisher.display || '';
    const meta = parseSfuDisplay(display);
    sfuFeedMeta.set(feedId, meta);
    const handleId = await sfuAttachPlugin();
    const handle = { type: 'subscriber', handleId, feedId, pc: null, pendingCandidates: [] };
    sfuState.handles.set(handleId, handle);
    sfuState.subscribers.set(feedId, handle);
    await sfuSend({
        janus: 'message',
        session_id: sfuState.sessionId,
        handle_id: handleId,
        body: {
            request: 'join',
            room: sfuState.roomId,
            ptype: 'subscriber',
            feed: feedId
        }
    });
}

function detachSfuSubscriber(feedId) {
    const handle = sfuState.subscribers.get(feedId);
    if (!handle) return;
    const participantId = buildSfuParticipantId(feedId);
    sfuParticipantLabels.delete(participantId);
    sfuFeedMeta.delete(feedId);
    removeParticipant(participantId);
    if (handle.pc) {
        handle.pc.close();
        handle.pc = null;
    }
    if (sfuState.sessionId && handle.handleId) {
        sfuSend({
            janus: 'detach',
            session_id: sfuState.sessionId,
            handle_id: handle.handleId
        }).catch(() => {});
    }
    sfuState.handles.delete(handle.handleId);
    sfuState.subscribers.delete(feedId);
}

async function sfuConnect(janusUrl) {
    if (sfuState.active && sfuState.sessionId && sfuState.janusUrl === janusUrl) return true;
    resetSfuState();
    return new Promise((resolve, reject) => {
        let settled = false;
        const ws = new WebSocket(janusUrl);
        const timeoutId = setTimeout(() => {
            if (!settled) {
                settled = true;
                ws.close();
                reject(new Error('SFU socket timeout'));
            }
        }, 8000);
        ws.onopen = async () => {
            if (settled) return;
            sfuState.ws = ws;
            try {
                const response = await sfuSend({ janus: 'create' });
                const sessionId = response && response.data ? response.data.id : null;
                if (!sessionId) {
                    throw new Error('No session id');
                }
                sfuState.sessionId = sessionId;
                sfuState.active = true;
                sfuState.janusUrl = janusUrl;
                sfuState.keepaliveInterval = setInterval(() => {
                    if (!sfuState.sessionId) return;
                    sfuSend({ janus: 'keepalive', session_id: sfuState.sessionId }).catch(() => {});
                }, SFU_KEEPALIVE_INTERVAL_MS);
                settled = true;
                clearTimeout(timeoutId);
                resolve(true);
            } catch (err) {
                settled = true;
                clearTimeout(timeoutId);
                ws.close();
                reject(err);
            }
        };
        ws.onmessage = (event) => handleSfuMessage(event.data);
        ws.onclose = () => {
            if (!settled) {
                settled = true;
                clearTimeout(timeoutId);
                reject(new Error('SFU socket closed'));
            }
            stopSfuSession('socket-closed');
        };
        ws.onerror = () => {
            if (!settled) {
                settled = true;
                clearTimeout(timeoutId);
                reject(new Error('SFU socket error'));
            }
        };
    });
}

async function sfuAttachPlugin() {
    const response = await sfuSend({
        janus: 'attach',
        session_id: sfuState.sessionId,
        plugin: 'janus.plugin.videoroom'
    });
    const handleId = response && response.data ? response.data.id : null;
    if (!handleId) throw new Error('No handle id');
    return handleId;
}

async function sfuJoinAndPublish(localStream, callType) {
    const handleId = await sfuAttachPlugin();
    const handle = { type: 'publisher', handleId, pc: null };
    sfuState.publisher = handle;
    sfuState.handles.set(handleId, handle);
    sfuState.publisherDisplay = getLocalDisplayPayload();
    sfuState.publisherJoinPromise = new Promise(resolve => {
        sfuState.publisherJoinResolve = resolve;
    });
    await sfuSend({
        janus: 'message',
        session_id: sfuState.sessionId,
        handle_id: handleId,
        body: {
            request: 'join',
            ptype: 'publisher',
            room: sfuState.roomId,
            display: sfuState.publisherDisplay
        }
    });
    await Promise.race([
        sfuState.publisherJoinPromise,
        new Promise((_, reject) => {
            setTimeout(() => reject(new Error('SFU join timeout')), 6000);
        })
    ]);
    const pc = new RTCPeerConnection({ iceServers: buildIceServers() });
    handle.pc = pc;
    pc.onicecandidate = (event) => {
        if (!event.candidate) {
            sfuSend({
                janus: 'trickle',
                session_id: sfuState.sessionId,
                handle_id: handleId,
                candidate: { completed: true }
            }).catch(() => {});
            return;
        }
        sfuSend({
            janus: 'trickle',
            session_id: sfuState.sessionId,
            handle_id: handleId,
            candidate: event.candidate
        }).catch(() => {});
    };
    if (localStream) {
        localStream.getTracks().forEach(track => {
            pc.addTrack(track, localStream);
        });
    }
    const hasVideo = localStream && localStream.getVideoTracks().length > 0;
    if (!hasVideo) {
        ensureVideoTransceiverForScreenShare(pc);
    } else {
        const videoSender = findVideoSender(pc);
        await applyCameraEncodingForCall(videoSender);
    }
    const offer = await pc.createOffer({
        offerToReceiveAudio: false,
        offerToReceiveVideo: false
    });
    await pc.setLocalDescription(offer);
    await sfuSend({
        janus: 'message',
        session_id: sfuState.sessionId,
        handle_id: handleId,
        body: {
            request: 'publish',
            audio: true,
            video: true,
            data: false
        },
        jsep: offer
    });
}

async function stopSfuSession(reason) {
    if (!sfuState.active) return;
    clearSfuTransactions();
    if (sfuState.keepaliveInterval) {
        clearInterval(sfuState.keepaliveInterval);
        sfuState.keepaliveInterval = null;
    }
    sfuFeedMeta.forEach((_meta, feedId) => {
        const participantId = buildSfuParticipantId(feedId);
        removeParticipant(participantId);
    });
    if (sfuState.publisher && sfuState.publisher.pc) {
        sfuState.publisher.pc.close();
    }
    sfuState.subscribers.forEach(handle => {
        if (handle.pc) handle.pc.close();
    });
    if (sfuState.ws && sfuState.ws.readyState === WebSocket.OPEN && sfuState.sessionId) {
        try {
            await sfuSend({ janus: 'destroy', session_id: sfuState.sessionId });
        } catch (_) {}
        try {
            sfuState.ws.close();
        } catch (_) {}
    }
    resetSfuState();
}

async function startGroupCallSfu(groupId, callType, configOverride) {
    const config = configOverride || await fetchSfuRoomConfig(groupId);
    if (!config) return false;
    sfuState.roomId = config.roomId;
    sfuState.roomReady = config.roomReady;
    try {
        await sfuConnect(config.janusUrl);
        await sfuJoinAndPublish(localStream, callType);
        return true;
    } catch (err) {
        console.warn('[SFU] Failed to start:', err);
        await stopSfuSession('start-failed');
        return false;
    }
}

async function toggleScreenShareSfu() {
    if (!sfuState.publisher || !sfuState.publisher.pc) {
        return;
    }
    try {
        if (screenStream) {
            const hadScreenAudio = screenStream.getAudioTracks().length > 0;
            screenStream.getTracks().forEach(track => track.stop());
            screenStream = null;
            if (localStream) {
                const videoTracks = localStream.getVideoTracks();
                const videoSender = findVideoSender(sfuState.publisher.pc);
                if (videoTracks.length && videoSender) {
                    await videoSender.replaceTrack(videoTracks[0]);
                    await applyCameraEncodingForCall(videoSender);
                }
                if (hadScreenAudio) {
                    const audioTracks = localStream.getAudioTracks();
                    const audioSender = sfuState.publisher.pc.getSenders().find(s => s.track && s.track.kind === 'audio');
                    if (audioTracks.length && audioSender) {
                        await audioSender.replaceTrack(audioTracks[0]);
                    }
                }
                const localVideo = document.getElementById('localVideo');
                if (localVideo) localVideo.srcObject = localStream;
            }
        } else {
            if (remoteScreenShareActive) {
                notifications?.warning && notifications.warning('Собеседник уже делится экраном — остановите его демонстрацию, чтобы избежать зависаний.');
                return;
            }
            screenStream = await navigator.mediaDevices.getDisplayMedia(buildScreenShareConstraintsForCall());
            const screenQuality = getCallQualitySettingForCall('screen');
            const videoTrack = screenStream.getVideoTracks()[0];
            const audioTrack = screenStream.getAudioTracks()[0];
            if (audioTrack) {
                await enhanceAudioTrack(audioTrack, 'screenShare');
            }
            if (videoTrack && typeof videoTrack.contentHint === 'string') {
                videoTrack.contentHint = 'detail';
            }
            const videoSender = findVideoSender(sfuState.publisher.pc);
            if (videoSender && videoTrack) {
                await videoSender.replaceTrack(videoTrack);
                await applyScreenShareEncoding(videoSender, screenQuality);
            }
            if (audioTrack) {
                const audioSender = sfuState.publisher.pc.getSenders().find(s => s.track && s.track.kind === 'audio');
                if (audioSender) {
                    await audioSender.replaceTrack(audioTrack);
                }
            }
            const localVideo = document.getElementById('localVideo');
            if (localVideo) {
                localVideo.srcObject = screenStream;
            }
            if (videoTrack) {
                videoTrack.onended = () => {
                    toggleScreenShareSfu();
                };
            }
        }

        const buttons = document.querySelectorAll('#toggleScreenBtn, #toggleScreenBtnGroup');
        buttons.forEach(btn => {
            btn.style.opacity = screenStream ? '1' : '0.5';
            btn.innerHTML = screenStream ? callIcon('screenOff') : callIcon('screen');
            btn.title = screenStream ? 'Остановить демонстрацию' : 'Демонстрация экрана';
        });
    } catch (error) {
        console.error('Ошибка демонстрации экрана (SFU):', error);
        if (error.name !== 'NotAllowedError') {
            notifications.error('Не удалось начать демонстрацию экрана');
        }
    }
}

async function loadTurnConfigOnce() {
    const isExpired = isTurnConfigExpired();
    if (turnConfigPromise && !isExpired) return turnConfigPromise;
    turnConfigPromise = (async () => {
        const applyNormalizedIce = (normalized, expiresAtSec) => {
            if (!normalized || !normalized.length) return false;
            cachedIceServers = normalized;
            if (Number.isFinite(expiresAtSec) && expiresAtSec > 0) {
                turnConfigExpiresAt = expiresAtSec * 1000;
            } else {
                turnConfigExpiresAt = 0;
            }
            const turnEntry = normalized.find(s => s.username && s.credential);
            if (turnEntry) {
                window.XIPHER_TURN = {
                    urls: turnEntry.urls,
                    username: turnEntry.username,
                    credential: turnEntry.credential
                };
            }
            return true;
        };

        const tryTurnCredentials = async () => {
            const userId = localStorage.getItem('xipher_user_id');
            if (!userId) return false;
            const resp = await fetch('/api/turn-credentials', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ userId, ttlMinutes: 60 })
            });
            if (!resp.ok) {
                throw new Error(`HTTP ${resp.status}`);
            }
            const data = await resp.json();
            if (data && data.username && data.credential && data.urls) {
                const normalized = normalizeIceServers([{
                    urls: data.urls,
                    username: data.username,
                    credential: data.credential
                }]);
                const expiresAt = Number(data.expires);
                return applyNormalizedIce(normalized, expiresAt);
            }
            return false;
        };

        const tryLegacyTurnConfig = async () => {
            const token = localStorage.getItem('xipher_token');
            if (!token) return false;
            const resp = await fetch('/api/turn-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token })
            });
            if (!resp.ok) {
                throw new Error(`HTTP ${resp.status}`);
            }
            const data = await resp.json();
            if (data && data.success && Array.isArray(data.ice_servers)) {
                const normalized = normalizeIceServers(data.ice_servers);
                return applyNormalizedIce(normalized, 0);
            }
            return false;
        };

        try {
            const ok = await tryTurnCredentials();
            if (ok) return;
        } catch (error) {
            console.error('[TURN] Failed to load TURN credentials:', error);
        }

        try {
            const ok = await tryLegacyTurnConfig();
            if (ok) return;
        } catch (error) {
            console.error('[TURN] Failed to load TURN config:', error);
        }

        if (isExpired) {
            clearTurnCache();
        }
    })();
    return turnConfigPromise;
}

function buildIceServers() {
    if (isTurnConfigExpired()) {
        clearTurnCache();
    }
    const seen = new Set();
    const ice = [];
    const pushServer = (entry) => {
        const [normalized] = normalizeIceServers([entry]);
        if (!normalized || !normalized.urls || !normalized.urls.length) return;
        const key = `${normalized.urls.join('|')}|${normalized.username || ''}`;
        if (seen.has(key)) return;
        seen.add(key);
        ice.push(normalized);
    };

    if (Array.isArray(cachedIceServers)) {
        cachedIceServers.forEach(pushServer);
    }

    BASE_ICE_SERVERS.forEach(pushServer);

    const runtimeTurn = (!isTurnConfigExpired()
        && typeof window !== 'undefined'
        && window?.XIPHER_TURN
        && window.XIPHER_TURN.username
        && window.XIPHER_TURN.credential)
        ? window.XIPHER_TURN
        : null;
    const configuredTurn = TURN_TEMPLATE.username !== 'REPLACE_ME' ? TURN_TEMPLATE : null;
    const turn = runtimeTurn || configuredTurn;
    if (turn) {
        pushServer(turn);
    }

    return ice;
}

function isTurnConfigExpired() {
    if (!turnConfigExpiresAt) return false;
    return Date.now() >= (turnConfigExpiresAt - TURN_REFRESH_SKEW_MS);
}

function clearTurnCache() {
    cachedIceServers = null;
    turnConfigExpiresAt = 0;
    if (typeof window !== 'undefined' && window?.XIPHER_TURN) {
        delete window.XIPHER_TURN;
    }
}

// Универсальный backoff с джиттером
async function withBackoff(task, { retries = 4, baseDelayMs = 500, jitterMs = 200 } = {}) {
    let attempt = 0;
    while (true) {
        try {
            return await task();
        } catch (err) {
            attempt += 1;
            if (attempt > retries) {
                throw err;
            }
            const backoff = baseDelayMs * Math.pow(2, attempt - 1);
            const jitter = Math.random() * jitterMs;
            const wait = backoff + jitter;
            console.warn(`[Retry] attempt ${attempt}/${retries} failed: ${err.message}; retry in ${wait.toFixed(0)}ms`);
            await new Promise(res => setTimeout(res, wait));
        }
    }
}

async function getUserMediaWithRetry(constraints, options = {}) {
    return withBackoff(() => getUserMediaWithRecovery(constraints), { retries: options.retries ?? 4, baseDelayMs: options.baseDelayMs ?? 500, jitterMs: options.jitterMs ?? 200 });
}

function buildAudioEnhancementConstraints(useFallback = false) {
    const supported = navigator.mediaDevices?.getSupportedConstraints?.() || {};
    const constraints = {};
    const add = (key, value) => {
        if (!supported || supported[key]) {
            constraints[key] = value;
        }
    };
    if (useFallback) {
        add('echoCancellation', true);
        add('noiseSuppression', true);
        add('autoGainControl', true);
        return constraints;
    }
    add('echoCancellation', { ideal: true });
    add('noiseSuppression', { ideal: true });
    add('autoGainControl', { ideal: true });
    add('channelCount', { ideal: 1 });
    add('sampleRate', { ideal: 48000 });
    add('sampleSize', { ideal: 16 });
    add('latency', { ideal: 0.02 });
    return constraints;
}

async function enhanceAudioTrack(track, label = 'audio') {
    if (!track || track.kind !== 'audio' || typeof track.applyConstraints !== 'function') return false;
    const constraints = buildAudioEnhancementConstraints(false);
    const keys = Object.keys(constraints);
    if (!keys.length) return false;
    try {
        await track.applyConstraints(constraints);
        console.log('[Call] audio constraints applied:', label, constraints);
        return true;
    } catch (err) {
        console.warn('[Call] audio constraints failed:', label, err?.name || err);
    }
    const fallback = buildAudioEnhancementConstraints(true);
    const fallbackKeys = Object.keys(fallback);
    if (!fallbackKeys.length) return false;
    try {
        await track.applyConstraints(fallback);
        console.log('[Call] audio constraints fallback applied:', label, fallback);
        return true;
    } catch (err) {
        console.warn('[Call] audio constraints fallback failed:', label, err?.name || err);
    }
    return false;
}

async function enhanceLocalAudioStream(stream, label = 'local') {
    if (!stream || typeof stream.getAudioTracks !== 'function') return;
    const tracks = stream.getAudioTracks();
    if (!tracks.length) return;
    for (const track of tracks) {
        await enhanceAudioTrack(track, label);
    }
}

const LOCAL_AUDIO_PROCESSING_DEFAULTS = {
    enabled: true,
    highPassHz: 80,
    lowPassHz: 8000,
    makeupGain: 1.0,
    compressor: {
        threshold: -24,
        knee: 30,
        ratio: 4,
        attack: 0.003,
        release: 0.25
    },
    noiseGate: {
        enabled: true,
        openThreshold: 0.015,
        closeThreshold: 0.008,
        minGain: 0.06,
        holdMs: 150,
        attackMs: 25,
        releaseMs: 160,
        intervalMs: 50,
        fftSize: 1024
    },
    rnnoise: {
        enabled: true,
        url: RNNOISE_DEFAULT_URL,
        bufferSize: 512,
        pcmScale: 32768
    }
};

function getLocalAudioProcessingConfig() {
    if (typeof window === 'undefined') {
        return LOCAL_AUDIO_PROCESSING_DEFAULTS;
    }
    const overrides = window.XIPHER_AUDIO_PROCESSING;
    if (overrides === false) {
        return { ...LOCAL_AUDIO_PROCESSING_DEFAULTS, enabled: false };
    }
    if (!overrides || typeof overrides !== 'object') {
        return LOCAL_AUDIO_PROCESSING_DEFAULTS;
    }
    return {
        ...LOCAL_AUDIO_PROCESSING_DEFAULTS,
        ...overrides,
        compressor: {
            ...LOCAL_AUDIO_PROCESSING_DEFAULTS.compressor,
            ...(overrides.compressor || {})
        },
        noiseGate: {
            ...LOCAL_AUDIO_PROCESSING_DEFAULTS.noiseGate,
            ...(overrides.noiseGate || {})
        },
        rnnoise: {
            ...LOCAL_AUDIO_PROCESSING_DEFAULTS.rnnoise,
            ...(overrides.rnnoise || {})
        }
    };
}

function clampNumber(value, min, max) {
    const num = Number(value);
    if (Number.isNaN(num)) return min;
    return Math.min(max, Math.max(min, num));
}

function volumeLevelFromPercent(percent) {
    return clampNumber(Number(percent) / 100, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
}

function volumePercentFromLevel(level) {
    return Math.round(clampNumber(level, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX) * 100);
}

function updateVolumeLabel(id, percent) {
    const el = document.getElementById(id);
    if (el) {
        el.textContent = `${percent}%`;
    }
}

function applyMicVolumeLevel() {
    if (localAudioNodes?.makeupGain) {
        const baseGain = localAudioNodes.makeupBase ?? 1;
        const target = baseGain * micVolumeLevel;
        if (localAudioContext) {
            localAudioNodes.makeupGain.gain.setTargetAtTime(target, localAudioContext.currentTime, 0.01);
        } else {
            localAudioNodes.makeupGain.gain.value = target;
        }
    }
}

function applyRemoteOutputVolume() {
    const remoteAudio = document.getElementById('remoteAudio');
    const remoteVideo = document.getElementById('remoteVideo');
    const isMuted = remoteAudio ? remoteAudio.muted : remoteVideo ? remoteVideo.muted : false;
    const targetLevel = isMuted ? 0 : headphoneVolumeLevel;
    const elementVolume = Math.min(1, targetLevel);
    if (remoteAudioGain && remoteAudioContext) {
        remoteAudioGain.gain.setTargetAtTime(targetLevel, remoteAudioContext.currentTime, 0.01);
    }
    if (remoteAudio) {
        remoteAudio.volume = elementVolume;
    }
    if (remoteVideo) {
        remoteVideo.volume = elementVolume;
    }
    const participantVideos = document.querySelectorAll('[id^="participant-video-"]');
    participantVideos.forEach(video => {
        video.volume = elementVolume;
    });
}

function setMicVolumePercent(percent) {
    micVolumeLevel = volumeLevelFromPercent(percent);
    const micPercent = volumePercentFromLevel(micVolumeLevel);
    updateVolumeLabel('micVolumeValue', micPercent);
    const slider = document.getElementById('micVolumeSlider');
    if (slider) {
        slider.value = String(micPercent);
    }
    applyMicVolumeLevel();
}

function setHeadphoneVolumePercent(percent) {
    headphoneVolumeLevel = volumeLevelFromPercent(percent);
    const hpPercent = volumePercentFromLevel(headphoneVolumeLevel);
    updateVolumeLabel('headphoneVolumeValue', hpPercent);
    const slider = document.getElementById('headphoneVolumeSlider');
    if (slider) {
        slider.value = String(hpPercent);
    }
    applyRemoteOutputVolume();
}

function syncVolumeSliders() {
    setMicVolumePercent(volumePercentFromLevel(micVolumeLevel));
    setHeadphoneVolumePercent(volumePercentFromLevel(headphoneVolumeLevel));
}

async function loadRnnoiseInstance(url = RNNOISE_DEFAULT_URL) {
    const resolvedUrl = url || RNNOISE_DEFAULT_URL;
    if (rnnoiseInstancePromise && rnnoiseInstanceUrl === resolvedUrl) {
        return rnnoiseInstancePromise;
    }
    rnnoiseInstanceUrl = resolvedUrl;
    rnnoiseInstancePromise = (async () => {
        try {
            const mod = await import(resolvedUrl);
            if (!mod || !mod.Rnnoise || typeof mod.Rnnoise.load !== 'function') {
                throw new Error('Rnnoise export missing');
            }
            return await mod.Rnnoise.load();
        } catch (err) {
            console.warn('[Call] RNNoise load failed:', err);
            rnnoiseInstancePromise = null;
            return null;
        }
    })();
    return rnnoiseInstancePromise;
}

function stopLocalAudioProcessing(reason = 'unknown') {
    if (localNoiseGateInterval) {
        clearInterval(localNoiseGateInterval);
        localNoiseGateInterval = null;
    }
    localNoiseGateState = { open: false, lastOpenAt: 0 };
    if (rnnoiseNode) {
        try {
            rnnoiseNode.onaudioprocess = null;
            rnnoiseNode.disconnect();
        } catch (err) {
            console.warn('[Call] RNNoise node cleanup failed:', reason, err);
        }
        rnnoiseNode = null;
    }
    if (rnnoiseState) {
        try {
            rnnoiseState.destroy();
        } catch (err) {
            console.warn('[Call] RNNoise state cleanup failed:', reason, err);
        }
        rnnoiseState = null;
    }
    if (localAudioNodes) {
        try {
            Object.values(localAudioNodes).forEach(node => {
                if (node && typeof node.disconnect === 'function') {
                    node.disconnect();
                }
            });
        } catch (err) {
            console.warn('[Call] Audio processing disconnect failed:', reason, err);
        }
        localAudioNodes = null;
    }
    if (localAudioContext) {
        const ctx = localAudioContext;
        localAudioContext = null;
        if (typeof ctx.close === 'function') {
            ctx.close().catch(err => {
                console.warn('[Call] Audio processing close failed:', reason, err);
            });
        }
    }
}

function startLocalNoiseGate(analyser, gateGain, ctx, gateConfig) {
    if (!gateConfig || !gateConfig.enabled) return;
    if (!analyser || !gateGain || !ctx) return;
    if (localNoiseGateInterval) {
        clearInterval(localNoiseGateInterval);
    }
    const buffer = new Float32Array(analyser.fftSize);
    localNoiseGateState = { open: false, lastOpenAt: 0 };
    const intervalMs = gateConfig.intervalMs || 50;
    localNoiseGateInterval = setInterval(() => {
        try {
            analyser.getFloatTimeDomainData(buffer);
            let sum = 0;
            for (let i = 0; i < buffer.length; i += 1) {
                const sample = buffer[i];
                sum += sample * sample;
            }
            const rms = Math.sqrt(sum / buffer.length);
            const now = (typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now();
            if (rms >= gateConfig.openThreshold) {
                localNoiseGateState.open = true;
                localNoiseGateState.lastOpenAt = now;
            } else if (localNoiseGateState.open && rms < gateConfig.closeThreshold) {
                if (now - localNoiseGateState.lastOpenAt > gateConfig.holdMs) {
                    localNoiseGateState.open = false;
                }
            }
            const target = localNoiseGateState.open ? 1 : gateConfig.minGain;
            const timeConstant = (localNoiseGateState.open ? gateConfig.attackMs : gateConfig.releaseMs) / 1000;
            gateGain.gain.setTargetAtTime(target, ctx.currentTime, timeConstant);
        } catch (err) {
            console.warn('[Call] Noise gate update failed:', err);
        }
    }, intervalMs);
}

async function createRnnoiseProcessor(ctx, rnnoiseConfig = {}) {
    if (!rnnoiseConfig.enabled) return null;
    if (!ctx || typeof ctx.createScriptProcessor !== 'function') {
        console.warn('[Call] RNNoise skipped: ScriptProcessor not available');
        return null;
    }
    const rnnoise = await loadRnnoiseInstance(rnnoiseConfig.url);
    if (!rnnoise) return null;
    if (ctx.sampleRate !== 48000) {
        console.warn('[Call] RNNoise skipped: unsupported sampleRate', ctx.sampleRate);
        return null;
    }
    let denoiseState;
    try {
        denoiseState = rnnoise.createDenoiseState();
    } catch (err) {
        console.warn('[Call] RNNoise state init failed:', err);
        return null;
    }
    const frameSize = rnnoise.frameSize || 480;
    const bufferSize = rnnoiseConfig.bufferSize || 1024;
    const pcmScale = rnnoiseConfig.pcmScale || 32768;
    let processor;
    try {
        processor = ctx.createScriptProcessor(bufferSize, 1, 1);
    } catch (err) {
        console.warn('[Call] RNNoise processor init failed:', err);
        try {
            denoiseState.destroy();
        } catch (cleanupErr) {
            console.warn('[Call] RNNoise destroy failed:', cleanupErr);
        }
        return null;
    }
    let inputBuffer = new Float32Array(bufferSize + frameSize);
    let inputLength = 0;
    let outputBuffer = new Float32Array(bufferSize + frameSize * 2);
    let outputLength = 0;
    let outputReady = false;

    processor.onaudioprocess = (event) => {
        const input = event.inputBuffer.getChannelData(0);
        if (inputLength + input.length > inputBuffer.length) {
            const next = new Float32Array(inputLength + input.length);
            next.set(inputBuffer.subarray(0, inputLength));
            inputBuffer = next;
        }
        inputBuffer.set(input, inputLength);
        inputLength += input.length;

        let offset = 0;
        while (inputLength - offset >= frameSize) {
            const frame = inputBuffer.subarray(offset, offset + frameSize);
            for (let i = 0; i < frame.length; i += 1) {
                let sample = frame[i];
                if (sample > 1) sample = 1;
                if (sample < -1) sample = -1;
                frame[i] = sample * pcmScale;
            }
            try {
                denoiseState.processFrame(frame);
            } catch (err) {
                console.warn('[Call] RNNoise frame failed:', err);
                break;
            }
            if (outputLength + frameSize > outputBuffer.length) {
                const nextOut = new Float32Array(outputLength + frameSize);
                nextOut.set(outputBuffer.subarray(0, outputLength));
                outputBuffer = nextOut;
            }
            for (let i = 0; i < frame.length; i += 1) {
                let sample = frame[i] / pcmScale;
                if (sample > 1) sample = 1;
                if (sample < -1) sample = -1;
                outputBuffer[outputLength + i] = sample;
            }
            outputLength += frameSize;
            offset += frameSize;
        }
        if (offset > 0) {
            inputBuffer.copyWithin(0, offset, inputLength);
            inputLength -= offset;
        }

        const output = event.outputBuffer.getChannelData(0);
        if (!outputReady) {
            if (outputLength < output.length) {
                output.fill(0);
                return;
            }
            outputReady = true;
        }
        if (outputLength >= output.length) {
            output.set(outputBuffer.subarray(0, output.length));
            outputBuffer.copyWithin(0, output.length, outputLength);
            outputLength -= output.length;
        } else {
            output.set(outputBuffer.subarray(0, outputLength));
            output.fill(0, outputLength);
            outputLength = 0;
        }
    };

    rnnoiseState = denoiseState;
    rnnoiseNode = processor;
    return processor;
}

async function buildProcessedLocalAudioStream(inputStream, label = 'local') {
    const config = getLocalAudioProcessingConfig();
    if (!config.enabled || !inputStream || typeof inputStream.getAudioTracks !== 'function') return null;
    if (inputStream.getAudioTracks().length === 0) return null;
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return null;

    stopLocalAudioProcessing(`rebuild-${label}`);

    let ctx;
    try {
        ctx = new Ctx({ latencyHint: 'interactive', sampleRate: 48000 });
    } catch (err) {
        try {
            ctx = new Ctx({ latencyHint: 'interactive' });
        } catch (fallbackErr) {
            console.warn('[Call] AudioContext init failed:', fallbackErr);
            return null;
        }
    }

    localAudioContext = ctx;
    if (ctx.state === 'suspended') {
        try {
            await ctx.resume();
        } catch (err) {
            console.warn('[Call] AudioContext resume failed:', err);
        }
    }

    const source = ctx.createMediaStreamSource(inputStream);
    const highPass = ctx.createBiquadFilter();
    highPass.type = 'highpass';
    highPass.frequency.value = config.highPassHz;
    const lowPass = ctx.createBiquadFilter();
    lowPass.type = 'lowpass';
    lowPass.frequency.value = config.lowPassHz;
    const compressor = ctx.createDynamicsCompressor();
    compressor.threshold.value = config.compressor.threshold;
    compressor.knee.value = config.compressor.knee;
    compressor.ratio.value = config.compressor.ratio;
    compressor.attack.value = config.compressor.attack;
    compressor.release.value = config.compressor.release;
    const rnnoiseProcessor = await createRnnoiseProcessor(ctx, config.rnnoise);
    const analyser = ctx.createAnalyser();
    analyser.fftSize = config.noiseGate.fftSize;
    const gateGain = ctx.createGain();
    gateGain.gain.value = 1;
    const makeupGain = ctx.createGain();
    makeupGain.gain.value = config.makeupGain * micVolumeLevel;
    const destination = ctx.createMediaStreamDestination();

    source.connect(highPass);
    highPass.connect(lowPass);
    if (rnnoiseProcessor) {
        lowPass.connect(rnnoiseProcessor);
        rnnoiseProcessor.connect(compressor);
    } else {
        lowPass.connect(compressor);
    }
    compressor.connect(analyser);
    analyser.connect(gateGain);
    gateGain.connect(makeupGain);
    makeupGain.connect(destination);

    localAudioNodes = {
        source,
        highPass,
        lowPass,
        compressor,
        analyser,
        gateGain,
        makeupGain,
        destination,
        makeupBase: config.makeupGain
    };

    startLocalNoiseGate(analyser, gateGain, ctx, config.noiseGate);

    return destination.stream;
}

async function buildLocalStreamWithProcessing(rawStream, label = 'local') {
    if (!rawStream) return null;
    const processedStream = await buildProcessedLocalAudioStream(rawStream, label);
    if (!processedStream) return rawStream;
    const output = new MediaStream();
    const processedAudio = processedStream.getAudioTracks()[0];
    if (processedAudio) {
        output.addTrack(processedAudio);
    }
    rawStream.getVideoTracks().forEach(track => output.addTrack(track));
    return output;
}

// Сторож соединения по getStats (байты/потери/джиттер) + ICE restart
const connectionWatchdogs = new WeakMap();
function stopConnectionWatchdog(pc) {
    const intervalId = connectionWatchdogs.get(pc);
    if (intervalId) {
        clearInterval(intervalId);
        connectionWatchdogs.delete(pc);
    }
}
function restartIceSafe(pc) {
    if (!pc) return;
    try {
        if (typeof pc.restartIce === 'function') {
            pc.restartIce();
            console.warn('[ICE] restartIce triggered');
        } else {
            console.warn('[ICE] restartIce not supported in this browser');
        }
    } catch (err) {
        console.error('[ICE] restart failed:', err);
    }
}
function setSingleCallStatusText(text) {
    const statusElement = document.getElementById('callStatus') || document.querySelector('.call-status');
    if (statusElement) {
        statusElement.textContent = text;
    }
}
function restoreActiveCallStatusText() {
    if (!isCallActive || isGroupCall) return;
    const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
    setSingleCallStatusText(callTypeText);
}
function resetCallRecoveryState() {
    if (callRecoveryState.timer) {
        clearInterval(callRecoveryState.timer);
    }
    callRecoveryState.active = false;
    callRecoveryState.startedAt = 0;
    callRecoveryState.attempts = 0;
    callRecoveryState.timer = null;
    callRecoveryState.inFlight = false;
    callRecoveryState.noticeSent = false;
}
function clearCallRecoveryState(reason = '') {
    if (!callRecoveryState.active && !callRecoveryState.timer) return;
    resetCallRecoveryState();
    restoreActiveCallStatusText();
    if (reason) {
        console.log('[Call] Recovery cleared:', reason);
    }
}
async function sendIceRestartOffer(pc) {
    if (!pc || !isCallActive || isGroupCall) return false;
    if (!isCaller) return false;
    if (!currentChat || !currentChat.id) return false;
    if (callRecoveryState.inFlight) return false;
    if (callRecoveryState.attempts >= CALL_RECOVERY_CONFIG.maxAttempts) return false;
    if (pc.signalingState !== 'stable') return false;

    callRecoveryState.inFlight = true;
    try {
        if (typeof pc.restartIce === 'function') {
            pc.restartIce();
        }
        const wantsVideo = currentCallType === 'video' || !!findVideoTransceiver(pc);
        const rawOffer = await pc.createOffer({
            iceRestart: true,
            offerToReceiveAudio: true,
            offerToReceiveVideo: wantsVideo
        });
        const filteredOffer = {
            type: rawOffer.type,
            sdp: stripCodecs(rawOffer.sdp || '')
        };
        await pc.setLocalDescription(filteredOffer);
        const payloadOffer = {
            ...filteredOffer,
            __xipher: { reoffer: true, ts: Date.now() }
        };
        await sendOffer(payloadOffer);
        callRecoveryState.attempts += 1;
        return true;
    } catch (err) {
        console.warn('[Call] ICE re-offer failed:', err);
        return false;
    } finally {
        callRecoveryState.inFlight = false;
    }
}
function beginCallRecovery(pc, reason = '') {
    if (!pc || !isCallActive || isGroupCall) return;
    if (!callRecoveryState.active) {
        callRecoveryState.active = true;
        callRecoveryState.startedAt = Date.now();
        callRecoveryState.attempts = 0;
        setSingleCallStatusText('Восстанавливаем соединение...');
    }
    if (!callRecoveryState.noticeSent) {
        notifications?.warning && notifications.warning('Пробуем восстановить соединение...');
        callRecoveryState.noticeSent = true;
    }
    if (callRecoveryState.timer) return;
    callRecoveryState.timer = setInterval(() => {
        if (!isCallActive) {
            resetCallRecoveryState();
            return;
        }
        if (!peerConnection || peerConnection.connectionState === 'closed') {
            resetCallRecoveryState();
            return;
        }
        if (peerConnection.connectionState === 'connected'
            || peerConnection.iceConnectionState === 'connected'
            || peerConnection.iceConnectionState === 'completed') {
            clearCallRecoveryState('connected');
            return;
        }
        const elapsed = Date.now() - callRecoveryState.startedAt;
        if (elapsed >= CALL_RECOVERY_CONFIG.graceMs) {
            console.warn('[Call] Recovery timeout, ending call');
            resetCallRecoveryState();
            endCall();
            return;
        }
        sendIceRestartOffer(peerConnection);
    }, CALL_RECOVERY_CONFIG.retryMs);
    if (reason) {
        console.log('[Call] Recovery started:', reason);
    }
}
function startConnectionWatchdog(pc, label = 'call') {
    stopConnectionWatchdog(pc);
    if (!pc) return;
    const intervalId = setInterval(async () => {
        try {
            if (pc.connectionState === 'closed') {
                stopConnectionWatchdog(pc);
                return;
            }
            const stats = await pc.getStats();
            let inbound = null;
            stats.forEach(report => {
                if (report.type === 'inbound-rtp' && report.kind === 'audio') {
                    inbound = report;
                }
            });
            if (!inbound) return;
            const bytes = inbound.bytesReceived || 0;
            const packets = inbound.packetsReceived || 0;
            const loss = inbound.packetsLost || 0;
            const jitter = inbound.jitter || 0;

            const prev = pc.__watchdogPrev || { bytes: 0, packets: 0 };
            const bytesGrowing = bytes > prev.bytes;
            const packetsGrowing = packets > prev.packets;
            pc.__watchdogPrev = { bytes, packets };

            const lossRatio = packets > 0 ? loss / packets : 0;
            const stalled = !bytesGrowing && !packetsGrowing;

            console.debug('[Watchdog]', label, {
                bytes,
                packets,
                loss: (lossRatio * 100).toFixed(1) + '%',
                jitter: jitter.toFixed(4),
                connectionState: pc.connectionState,
                iceState: pc.iceConnectionState,
                stalled
            });

            if (lossRatio > 0.05) {
                console.warn('[Watchdog] High packet loss detected', lossRatio);
            }
            if (jitter > 0.05) {
                console.warn('[Watchdog] High jitter detected', jitter);
            }
            if (stalled) {
                pc.__stallCount = (pc.__stallCount || 0) + 1;
                if (pc.__stallCount >= 2) {
                    console.error('[Watchdog] stall detected, restarting ICE');
                    restartIceSafe(pc);
                    pc.__stallCount = 0;
                }
            } else {
                pc.__stallCount = 0;
            }
        } catch (err) {
            console.warn('[Watchdog] stats failed:', err);
        }
    }, 3000);
    connectionWatchdogs.set(pc, intervalId);
}

// Укорачиваем SDP, оставляя только основные аудио-кодеки (opus + DTMF), видео не трогаем
function stripCodecs(sdp) {
    try {
        const lines = sdp.split(/\r\n/);
        const keepAudio = ['opus', 'telephone-event'];
        const rtpmap = {};
        const keepPayloads = { audio: new Set() };

        lines.forEach(line => {
            if (line.startsWith('a=rtpmap:')) {
                const m = line.match(/^a=rtpmap:(\d+)\s+([A-Za-z0-9\-\+]+)/);
                if (m) {
                    rtpmap[m[1]] = m[2];
                }
            }
        });

        Object.entries(rtpmap).forEach(([pt, codec]) => {
            const lc = codec.toLowerCase();
            if (keepAudio.some(c => lc.startsWith(c.toLowerCase()))) {
                keepPayloads.audio.add(pt);
            }
        });

        const filterAudioLines = (line) => {
            if (line.startsWith('a=rtpmap:') || line.startsWith('a=fmtp:') || line.startsWith('a=rtcp-fb:')) {
                const m = line.match(/^a=(?:rtpmap|fmtp|rtcp-fb):(\d+)/);
                if (!m) return true;
                const pt = m[1];
                return keepPayloads.audio.has(pt);
            }
            return true;
        };

        const rebuildAudioMLine = (line) => {
            const parts = line.split(' ');
            const header = parts.slice(0, 3); // m=<media> <port> <proto>
            const payloads = [...keepPayloads.audio];
            if (payloads.length === 0) return line; // не трогаем, чтобы не сломать SDP
            return [...header, ...payloads].join(' ');
        };

        const out = [];
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            if (line.startsWith('m=audio')) {
                out.push(rebuildAudioMLine(line));
                i++;
                while (i < lines.length && !lines[i].startsWith('m=')) {
                    if (filterAudioLines(lines[i])) out.push(lines[i]);
                    i++;
                }
                i--; // compensate outer loop increment
            } else if (line.startsWith('m=video')) {
                // Видео оставляем как есть, чтобы не ломать согласование
                out.push(line);
                i++;
                while (i < lines.length && !lines[i].startsWith('m=')) {
                    out.push(lines[i]);
                    i++;
                }
                i--;
            } else {
                out.push(line);
            }
        }

        return out.join('\r\n');
    } catch (e) {
        console.warn('stripCodecs failed, returning original SDP:', e);
        return sdp;
    }
}

function ensureRemoteAudioElement() {
    let remoteAudio = document.getElementById('remoteAudio');
    if (!remoteAudio) {
        remoteAudio = document.createElement('audio');
        remoteAudio.id = 'remoteAudio';
        document.body.appendChild(remoteAudio);
    }
    if (!remoteAudio.parentElement) {
        document.body.appendChild(remoteAudio);
    }
    remoteAudio.autoplay = true;
    remoteAudio.playsInline = true;
    remoteAudio.preload = 'auto';
    remoteAudio.controls = false;
    remoteAudio.style.position = 'fixed';
    remoteAudio.style.left = '-10000px';
    remoteAudio.style.top = '0';
    remoteAudio.style.width = '1px';
    remoteAudio.style.height = '1px';
    remoteAudio.style.opacity = '0';
    remoteAudio.style.pointerEvents = 'none';
    remoteAudio.style.display = 'block';
    remoteAudio.muted = false;
    applyRemoteOutputVolume();
    return remoteAudio;
}

// FIX: Явно резюмируем AudioContext, чтобы не упираться в политику автоплея
async function resumeRemoteAudioContext(reason = 'unknown') {
    try {
        const Ctx = window.AudioContext || window.webkitAudioContext;
        if (!Ctx) return;
        if (!remoteAudioContext) {
            remoteAudioContext = new Ctx();
        }
        if (remoteAudioContext.state === 'suspended') {
            await remoteAudioContext.resume();
            console.log('[Call] AudioContext resumed via', reason);
        }
    } catch (err) {
        console.warn('[Call] AudioContext resume failed:', err);
    }
}

function setupRemoteAudioProcessing(stream) {
    if (!remoteAudioContext || !stream) return null;
    try {
        if (!remoteAudioGain) {
            remoteAudioGain = remoteAudioContext.createGain();
        }
        if (!remoteAudioDestination) {
            remoteAudioDestination = remoteAudioContext.createMediaStreamDestination();
        }
        remoteAudioGain.gain.value = headphoneVolumeLevel;
        if (remoteAudioSource) {
            remoteAudioSource.disconnect();
            remoteAudioSource = null;
        }
        remoteAudioSource = remoteAudioContext.createMediaStreamSource(stream);
        remoteAudioGain.disconnect();
        remoteAudioSource.connect(remoteAudioGain);
        remoteAudioGain.connect(remoteAudioDestination);
        remoteAudioProcessedStream = remoteAudioDestination.stream;
        return remoteAudioProcessedStream;
    } catch (err) {
        console.warn('[Call] Remote audio processing failed:', err);
        return null;
    }
}

function stopRemoteAudioProcessing(reason = 'unknown') {
    try {
        if (remoteAudioSource) {
            remoteAudioSource.disconnect();
            remoteAudioSource = null;
        }
        if (remoteAudioGain) {
            remoteAudioGain.disconnect();
            remoteAudioGain = null;
        }
    } catch (err) {
        console.warn('[Call] Remote audio cleanup failed:', reason, err);
    }
    remoteAudioDestination = null;
    remoteAudioProcessedStream = null;
}

function resolveRemoteStreamFromTrack(event) {
    if (event && event.streams && event.streams[0]) {
        return event.streams[0];
    }
    if (!remoteStream) {
        remoteStream = new MediaStream();
    }
    if (event && event.track && !remoteStream.getTracks().some(t => t.id === event.track.id)) {
        remoteStream.addTrack(event.track);
    }
    return remoteStream;
}

// FIX: Унифицированное прикрепление удаленного потока с защитой от ошибок воспроизведения
async function attachRemoteStream(stream) {
    if (!stream) return;

    const remoteAudio = ensureRemoteAudioElement();
    if (remoteAudio) {
        try {
            if (!remoteAudioStream) {
                remoteAudioStream = new MediaStream();
            }
            const audioTracks = stream.getAudioTracks();
            const wasMuted = remoteAudio.muted;
            remoteAudioStream.getTracks().forEach(track => remoteAudioStream.removeTrack(track));
            audioTracks.forEach(track => remoteAudioStream.addTrack(track));
            if (audioTracks.length > 0) {
                await resumeRemoteAudioContext('attachRemoteStream');
            }
            const processedStream = audioTracks.length > 0 ? setupRemoteAudioProcessing(remoteAudioStream) : null;
            const playbackStream = processedStream || remoteAudioStream;
            if (remoteAudio.srcObject !== playbackStream) {
                remoteAudio.srcObject = playbackStream;
            }
            remoteAudio.muted = wasMuted;
            applyRemoteOutputVolume();
            if (selectedAudioOutputId && typeof remoteAudio.setSinkId === 'function') {
                await remoteAudio.setSinkId(selectedAudioOutputId);
            }
            if (audioTracks.length > 0) {
                const playPromise = remoteAudio.play && remoteAudio.play();
                if (playPromise && typeof playPromise.catch === 'function') {
                    playPromise.catch(err => {
                        if (err.name !== 'AbortError') {
                            console.error('[Call] Ошибка воспроизведения удаленного аудио:', err);
                        }
                    });
                }
            } else {
                stopRemoteAudioProcessing('attachRemoteStream-empty');
            }
        } catch (err) {
            console.error('[Call] attachRemoteStream audio error:', err);
        }
    }

    const remoteVideo = document.getElementById('remoteVideo');
    if (remoteVideo) {
        try {
            if (remoteVideo.srcObject !== stream) {
                remoteVideo.srcObject = stream;
            }
            remoteVideo.muted = true;
            applyRemoteOutputVolume();
            const playPromise = remoteVideo.play && remoteVideo.play();
            if (playPromise && typeof playPromise.catch === 'function') {
                playPromise.catch(err => {
                    if (err.name !== 'AbortError') {
                        console.error('[Call] Ошибка воспроизведения удаленного видео:', err);
                    }
                });
            }
        } catch (err) {
            console.error('[Call] attachRemoteStream video error:', err);
        }
    }

    setTimeout(forceRemotePlayback, 120);
}

// FIX: Определяем, что это трек шаринга экрана
function isScreenTrack(track) {
    if (!track) return false;
    const label = (track.label || '').toLowerCase();
    const hint = (track.contentHint || '').toLowerCase();
    return label.includes('screen') || label.includes('display') || hint.includes('screen');
}

// FIX: Уведомление о шаринге экрана от собеседника (однократно)
function notifyRemoteScreenShare() {
    if (remoteScreenShareNotified) return;
    remoteScreenShareNotified = true;
    try {
        const name = currentChat && currentChat.name ? currentChat.name : 'Собеседник';
        if (typeof showCallNotification === 'function') {
            showCallNotification({
                message: `${name} начал(а) делиться экраном`,
                fromUserId: currentChat ? currentChat.id : null,
                isGroupCall: isGroupCall
            });
        }
    } catch (err) {
        console.warn('[Call] notifyRemoteScreenShare failed:', err);
    }
}

// FIX: Кнопка полноэкранного просмотра удаленного видео
function ensureRemoteFullscreenButton() {
    const remoteVideo = document.getElementById('remoteVideo');
    if (!remoteVideo) return null;
    const container = document.getElementById('singleCallContainer') || remoteVideo.parentElement;
    if (!container) return null;
    let btn = document.getElementById('remoteFullscreenBtn');
    if (!btn) {
        btn = document.createElement('button');
        btn.id = 'remoteFullscreenBtn';
        btn.className = 'fullscreen-toggle-btn';
        btn.type = 'button';
        btn.innerHTML = CALL_ICONS.maximize;
        btn.title = 'Развернуть';
        btn.style.position = 'absolute';
        btn.style.top = '10px';
        btn.style.right = '10px';
        btn.style.zIndex = '12000';
        btn.style.padding = '8px';
        btn.style.borderRadius = '6px';
        btn.style.border = 'none';
        btn.style.background = 'rgba(0,0,0,0.5)';
        btn.style.color = '#fff';
        btn.style.cursor = 'pointer';
        btn.style.backdropFilter = 'blur(4px)';
        btn.style.display = 'flex';
        btn.style.alignItems = 'center';
        btn.style.justifyContent = 'center';
        btn.addEventListener('click', () => toggleRemoteFullscreen(container));
        if (!container.style.position) {
            container.style.position = 'relative';
        }
        container.appendChild(btn);
    }
    return btn;
}

function toggleRemoteFullscreen(target) {
    if (!target) return;
    const el = document.fullscreenElement;
    if (!el) {
        const req = target.requestFullscreen || target.webkitRequestFullscreen || target.mozRequestFullScreen || target.msRequestFullscreen;
        if (req) req.call(target);
    } else {
        const exit = document.exitFullscreen || document.webkitExitFullscreen || document.mozCancelFullScreen || document.msExitFullscreen;
        if (exit) exit.call(document);
    }
}

document.addEventListener('fullscreenchange', () => {
    const btn = document.getElementById('remoteFullscreenBtn');
    if (btn) {
        const active = !!document.fullscreenElement;
        btn.innerHTML = active ? CALL_ICONS.minimize : CALL_ICONS.maximize;
        btn.title = active ? 'Свернуть' : 'Развернуть';
    }
});

function forceRemotePlayback() {
    const remoteAudio = ensureRemoteAudioElement();
    const remoteVideo = document.getElementById('remoteVideo');
    if (remoteAudio) {
        remoteAudio.muted = false;
        applyRemoteOutputVolume();
        const p = remoteAudio.play && remoteAudio.play();
        if (p && typeof p.catch === 'function') {
            p.catch(err => {
                if (err.name !== 'AbortError') {
                    console.error('[Call] forceRemotePlayback audio error:', err);
                }
            });
        }
    }
    if (remoteVideo) {
        remoteVideo.muted = true;
        applyRemoteOutputVolume();
        const p2 = remoteVideo.play && remoteVideo.play();
        if (p2 && typeof p2.catch === 'function') {
            p2.catch(err => {
                if (err.name !== 'AbortError') {
                    console.error('[Call] forceRemotePlayback video error:', err);
                }
            });
        }
    }
}

function primeRemoteAudioFromUserGesture() {
    // Вызывать из кнопок принятия/старта (есть пользовательский жест)
    const remoteAudio = ensureRemoteAudioElement();
    if (remoteAudio) {
        remoteAudio.muted = false;
        applyRemoteOutputVolume();
        // FIX: Резюмируем аудиоконтекст сразу после пользовательского жеста
        resumeRemoteAudioContext('user-gesture');
        const p = remoteAudio.play && remoteAudio.play();
        if (p && typeof p.catch === 'function') {
            p.catch(err => {
                if (err.name !== 'AbortError') {
                    console.error('[Call] primeRemoteAudio error:', err);
                }
            });
        }
    }
}

function disableCallUI() {
    const controls = document.querySelectorAll('[data-call-control], .call-action, .call-control-btn');
    controls.forEach(el => {
        if (!el.dataset.disabledByPermission) {
            el.dataset.disabledByPermission = 'true';
            el.disabled = true;
            el.classList.add('disabled');
        }
    });
}

function enableCallUI() {
    const controls = document.querySelectorAll('[data-call-control], .call-action, .call-control-btn');
    controls.forEach(el => {
        if (el.dataset.disabledByPermission) {
            delete el.dataset.disabledByPermission;
            el.disabled = false;
            el.classList.remove('disabled');
        }
    });
}

async function checkPermissionState() {
    try {
        const mic = await navigator.permissions.query({ name: 'microphone' });
        const cam = await navigator.permissions.query({ name: 'camera' });
        console.log('[Permissions]', { mic: mic.state, cam: cam.state });
        mic.addEventListener('change', () => console.log('[Permission Changed] mic:', mic.state));
        cam.addEventListener('change', () => console.log('[Permission Changed] cam:', cam.state));
    } catch (e) {
        console.warn('[Permissions API] Not supported:', e.message);
    }
}

function monitorPermissions() {
    if (permissionMonitorInterval) return;
    permissionMonitorInterval = setInterval(async () => {
        try {
            const mic = await navigator.permissions.query({ name: 'microphone' });
            if (mic.state === 'denied') {
                console.error('[Permission Monitor] Microphone denied');
                disableCallUI();
            } else if (mic.state === 'granted') {
                enableCallUI();
            }
        } catch (e) {
            console.warn('[Permission Monitor]', e);
        }
    }, 2000);
}

function showPermissionResetHint() {
    if (typeof notifications?.warning === 'function') {
        notifications.warning('Разрешения на микрофон/камеру заблокированы. Откройте настройки браузера, разрешите доступ для Xipher и обновите страницу.');
    } else {
        console.warn('[Permissions] blocked: попросите пользователя разрешить доступ в настройках браузера');
    }
}

function addRetryButton(onRetry) {
    const existing = document.getElementById('call-permission-retry');
    if (existing) return;
    const btn = document.createElement('button');
    btn.id = 'call-permission-retry';
    btn.textContent = 'Retry';
    btn.style.position = 'fixed';
    btn.style.bottom = '16px';
    btn.style.right = '16px';
    btn.style.zIndex = '99999';
    btn.onclick = async () => {
        const stream = await onRetry();
        if (stream) {
            btn.remove();
        }
    };
    document.body.appendChild(btn);
}

function cleanupMediaTracks(stream, label = 'cleanup') {
    if (!stream) return;
    stream.getTracks().forEach(track => {
        console.log('[Track Cleanup]', label, track.kind, track.readyState);
        track.stop();
    });
}

async function getUserMediaWithRecovery(constraints = STRICT_MEDIA_CONSTRAINTS) {
    try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        console.log('[gUM] success', constraints);
        return stream;
    } catch (err) {
        console.error('[gUM] failed:', err.name, err.message, constraints);
        if (err.name === 'OverconstrainedError' || err.name === 'ConstraintNotSatisfiedError') {
            if (constraints !== PERMISSIVE_MEDIA_CONSTRAINTS) {
                console.warn('[gUM] fallback to permissive constraints');
                return getUserMediaWithRecovery(PERMISSIVE_MEDIA_CONSTRAINTS);
            }
        }
        if (err.name === 'NotAllowedError') {
            showPermissionResetHint();
            addRetryButton(() => getUserMediaWithRecovery(constraints));
        }
        if (err.name === 'NotFoundError') {
            notifications?.error && notifications.error('Микрофон или камера не найдены');
        }
        return null;
    }
}

// Инициализируем отслеживание разрешений при загрузке
checkPermissionState();
monitorPermissions();

// FIX: Сторожевой таймер аудио — обнаруживает тишину и лечит
function startAudioWatchdog() {
    stopAudioWatchdog();
    audioWatchdogState = { lastBytes: 0, lastLevel: 0, silentSince: null, restartCount: 0 };
    audioWatchdogInterval = setInterval(async () => {
        try {
            if (!peerConnection || peerConnection.connectionState !== 'connected') return;
            const now = Date.now();
            let bytes = 0;
            let level = 0;
            const stats = await peerConnection.getStats();
            stats.forEach(report => {
                if (report.type === 'inbound-rtp' && report.kind === 'audio' && !report.isRemote) {
                    bytes += report.bytesReceived || 0;
                }
                if (report.type === 'track' && report.kind === 'audio') {
                    if (typeof report.audioLevel === 'number') {
                        level = Math.max(level, report.audioLevel);
                    }
                }
            });
            const bytesChanged = bytes > audioWatchdogState.lastBytes;
            const levelPresent = level > 0.0001; // ~ -80dB threshold
            if (!bytesChanged && !levelPresent) {
                if (!audioWatchdogState.silentSince) {
                    audioWatchdogState.silentSince = now;
                }
                const silentForMs = now - audioWatchdogState.silentSince;
                if (silentForMs > 3000) {
                    console.warn('[Call] Watchdog detected silence, applying self-heal');
                    // 1) Разбудить аудио контекст
                    await resumeRemoteAudioContext('watchdog');
                    // 2) Форс воспроизведение элементов
                    forceRemotePlayback();
                    // 3) Перекрепить поток
                    attachRemoteStream(remoteStream);
                    // 4) Перезапустить ICE не чаще 2 раз за сессию
                    if (typeof peerConnection.restartIce === 'function' && audioWatchdogState.restartCount < 2) {
                        try {
                            peerConnection.restartIce();
                            audioWatchdogState.restartCount += 1;
                            console.warn('[Call] Watchdog triggered ICE restart');
                        } catch (err) {
                            console.error('[Call] Watchdog ICE restart failed:', err);
                        }
                    }
                    audioWatchdogState.silentSince = now; // avoid rapid repeats
                }
            } else {
                audioWatchdogState.silentSince = null;
            }
            audioWatchdogState.lastBytes = bytes;
            audioWatchdogState.lastLevel = level;
        } catch (err) {
            console.error('[Call] Audio watchdog error:', err);
        }
    }, 2000);
}

function stopAudioWatchdog() {
    if (audioWatchdogInterval) {
        clearInterval(audioWatchdogInterval);
        audioWatchdogInterval = null;
    }
}

// Инициализация WebRTC
function initWebRTC() {
    // Проверяем поддержку WebRTC
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        console.error('WebRTC не поддерживается в этом браузере');
        notifications.error('WebRTC не поддерживается в этом браузере');
        return false;
    }
    return true;
}

// Получить список доступных устройств
async function enumerateDevices() {
    try {
        // Сначала нужно запросить доступ к микрофону/камере, чтобы получить названия устройств
        const stream = await getUserMediaWithRecovery(PERMISSIVE_MEDIA_CONSTRAINTS);
        if (stream) {
            cleanupMediaTracks(stream, 'enumerateDevices');
        } else {
            return null;
        }
        
        const devices = await navigator.mediaDevices.enumerateDevices();
        
        availableDevices.audioInputs = devices.filter(device => device.kind === 'audioinput');
        availableDevices.audioOutputs = devices.filter(device => device.kind === 'audiooutput');
        availableDevices.videoInputs = devices.filter(device => device.kind === 'videoinput');
        
        return availableDevices;
    } catch (error) {
        console.error('Ошибка получения списка устройств:', error);
        return null;
    }
}

// Выбрать устройство ввода аудио (микрофон)
async function selectAudioInput(deviceId) {
    selectedAudioInputId = deviceId;
    
    if (localStream) {
        const audioTracks = localStream.getAudioTracks();
        const videoTracks = localStream.getVideoTracks();
        const wasEnabled = audioTracks[0]?.enabled ?? true;
        try {
            // Создаем новый поток с выбранным микрофоном
            const newStream = await navigator.mediaDevices.getUserMedia({
                audio: { deviceId: { exact: deviceId } },
                video: videoTracks.length > 0 ? {
                    deviceId: videoTracks[0].getSettings().deviceId
                } : false
            });
            
            const newAudioTrack = newStream.getAudioTracks()[0];
            if (!newAudioTrack) {
                throw new Error('No audio track from selected device');
            }
            await enhanceAudioTrack(newAudioTrack, 'selectAudioInput');
            
            const nextRawStream = new MediaStream();
            nextRawStream.addTrack(newAudioTrack);
            const preservedVideoTracks = rawLocalStream && rawLocalStream.getVideoTracks().length > 0
                ? rawLocalStream.getVideoTracks()
                : videoTracks;
            preservedVideoTracks.forEach(track => nextRawStream.addTrack(track));
            
            const processedStream = await buildProcessedLocalAudioStream(nextRawStream, 'selectAudioInput');
            const finalAudioTrack = processedStream?.getAudioTracks()[0] || newAudioTrack;
            finalAudioTrack.enabled = wasEnabled;
            newAudioTrack.enabled = wasEnabled;
            
            // Обновляем одиночное соединение
            if (peerConnection) {
                const sender = peerConnection.getSenders().find(s => 
                    s.track && s.track.kind === 'audio'
                );
                if (sender) {
                    await sender.replaceTrack(finalAudioTrack);
                }
            }
            
            // Обновляем все групповые соединения
            if (isGroupCall && peerConnections.size > 0) {
                for (const [, pc] of peerConnections) {
                    const sender = pc.getSenders().find(s => 
                        s.track && s.track.kind === 'audio'
                    );
                    if (sender) {
                        await sender.replaceTrack(finalAudioTrack);
                    }
                }
            }
            
            // Останавливаем старые аудио треки
            audioTracks.forEach(track => {
                track.stop();
                localStream.removeTrack(track);
            });
            if (rawLocalStream) {
                rawLocalStream.getAudioTracks().forEach(track => track.stop());
            }
            rawLocalStream = nextRawStream;
            
            // Обновляем локальный поток
            localStream.addTrack(finalAudioTrack);
            
            // Обновляем видео элемент
            const localVideo = document.getElementById('localVideo');
            if (localVideo) {
                localVideo.srcObject = localStream;
            }
            
            // Останавливаем временные видео треки
            newStream.getVideoTracks().forEach(track => track.stop());
            
            notifications.success('Микрофон изменен');
        } catch (error) {
            console.error('Ошибка переключения микрофона:', error);
            notifications.error('Не удалось переключить микрофон');
        }
    }
}

// Выбрать устройство вывода аудио (динамики/наушники)
async function selectAudioOutput(deviceId) {
    selectedAudioOutputId = deviceId || null;
    const sinkId = selectedAudioOutputId || '';
    
    const remoteAudio = ensureRemoteAudioElement();
    const remoteVideo = document.getElementById('remoteVideo');
    const targets = [remoteAudio, remoteVideo];
    let hasSinkSupport = false;
    let applied = false;
    
    for (const target of targets) {
        if (!target || typeof target.setSinkId !== 'function') continue;
        hasSinkSupport = true;
        try {
            await target.setSinkId(sinkId);
            applied = true;
        } catch (error) {
            console.error('Ошибка переключения устройства вывода звука:', error);
        }
    }
    
    if (!hasSinkSupport) return;
    if (applied) {
        notifications.success('Устройство вывода звука изменено');
    } else {
        notifications.error('Не удалось переключить устройство вывода звука');
    }
}

function getCallTypeModalElements() {
    const modal = document.getElementById('callTypeModal');
    if (!modal) return null;
    return {
        modal,
        closeBtn: document.getElementById('closeCallTypeModal'),
        videoBtn: document.getElementById('callTypeVideoBtn'),
        audioBtn: document.getElementById('callTypeAudioBtn'),
        hint: document.getElementById('callTypeHint')
    };
}

function hideCallTypeModal() {
    const modal = document.getElementById('callTypeModal');
    if (modal) modal.style.display = 'none';
}

function ensureCallTypeModalSetup() {
    if (callTypeModalReady) return;
    const elements = getCallTypeModalElements();
    if (!elements) return;
    callTypeModalReady = true;

    const { modal, closeBtn, videoBtn, audioBtn } = elements;

    if (closeBtn) {
        closeBtn.addEventListener('click', hideCallTypeModal);
    }
    if (modal) {
        modal.addEventListener('click', event => {
            if (event.target === modal) {
                hideCallTypeModal();
            }
        });
    }
    document.addEventListener('keydown', event => {
        if (event.key === 'Escape') {
            hideCallTypeModal();
        }
    });

    if (videoBtn) {
        videoBtn.addEventListener('click', () => {
            if (videoBtn.disabled) return;
            hideCallTypeModal();
            startCall('video');
        });
    }
    if (audioBtn) {
        audioBtn.addEventListener('click', () => {
            hideCallTypeModal();
            startCall('audio');
        });
    }
}

function openCallTypeModal() {
    const activeGroup = window.groupsModule && typeof window.groupsModule.currentGroup === 'function'
        ? window.groupsModule.currentGroup()
        : (window.currentGroup || null);

    if (!currentChat && !activeGroup) {
        if (notifications?.warning) {
            notifications.warning('Выберите чат для звонка');
        } else {
            alert('Выберите чат для звонка');
        }
        return;
    }

    ensureCallTypeModalSetup();
    const elements = getCallTypeModalElements();
    if (!elements) {
        startCall('video');
        return;
    }

    const isGroupCandidate = !!activeGroup || (currentChat && currentChat.type === 'group');
    if (elements.videoBtn) {
        elements.videoBtn.disabled = isGroupCandidate;
        elements.videoBtn.setAttribute('aria-disabled', isGroupCandidate ? 'true' : 'false');
    }
    if (elements.hint) {
        elements.hint.style.display = isGroupCandidate ? 'block' : 'none';
    }

    elements.modal.style.display = 'flex';
}

function stripGroupVideoTracks(reason) {
    if (!localStream) return;
    localStream.getVideoTracks().forEach(track => {
        try { track.stop(); } catch (_) {}
        localStream.removeTrack(track);
        if (rawLocalStream) {
            rawLocalStream.removeTrack(track);
        }
    });
    if (reason) {
        console.log('[GroupCall] Video tracks stripped:', reason);
    }
}

// Начать звонок с гарантией доставки
async function startCall(callType = 'video') {
    console.log('[Call] startCall called, currentChat:', currentChat);
    
    callEndSent = false;
    lastIncomingOffer = null;
    lastIncomingOfferAt = 0;
    pendingIceCandidates = []; // Очищаем буфер ICE
    pendingIceCandidatesWithTs = [];
    
    // Ждём готовности WS/авторизации, чтобы не потерять offer/ICE
    if (typeof waitForWebSocketReady === 'function') {
        try {
            await waitForWebSocketReady(8000); // Увеличили таймаут
        } catch (e) {
            console.error('[Call] WebSocket not ready, aborting startCall:', e?.message || e);
            notifications?.error && notifications.error('Нет соединения WebSocket для звонка');
            return;
        }
    }
    
    // Загружаем TURN с retry
    try {
        await loadTurnConfigOnce();
    } catch (e) {
        console.warn('[Call] TURN load failed, proceeding with STUN only:', e);
    }
    
    // Проверяем конфигурацию ICE
    const iceServers = buildIceServers();
    console.log('[Call] ICE servers configured:', iceServers.length, iceServers.map(s => s.urls));
    
    // Проверяем, активна ли группа
    const activeGroup = window.groupsModule && typeof window.groupsModule.currentGroup === 'function'
        ? window.groupsModule.currentGroup()
        : (window.currentGroup || null);
    
    // Проверяем, является ли чат группой
    isGroupCall = activeGroup !== null || (currentChat && currentChat.type === 'group');
    
    if (!currentChat && !activeGroup) {
        notifications.warning('Выберите чат для звонка');
        return;
    }
    
    // Если это группа, используем activeGroup вместо currentChat
    if (isGroupCall && activeGroup) {
        // Временно устанавливаем currentChat для совместимости
        if (!currentChat) {
            currentChat = {
                id: activeGroup.id,
                name: activeGroup.name,
                type: 'group'
            };
        }
    }
    
    pendingGroupSfuConfig = null;
    if (isGroupCall) {
        const groupId = activeGroup ? activeGroup.id : (currentChat ? currentChat.id : null);
        if (groupId) {
            pendingGroupSfuConfig = await fetchSfuRoomConfig(groupId);
        }
    }

    if (!initWebRTC()) {
        return;
    }
    
    // Для групповых звонков принудительно используем аудио, чтобы снизить нагрузку
    const effectiveCallType = isGroupCall
        ? (pendingGroupSfuConfig ? callType : 'audio')
        : callType;
    currentCallType = effectiveCallType;
    isCaller = true;
    
    console.log('[Call] isGroupCall:', isGroupCall, 'activeGroup:', activeGroup, 'currentChat:', currentChat);

    // Праймим удалённый аудио-плеер жестом пользователя (кнопка вызова)
    primeRemoteAudioFromUserGesture();

    if (!isGroupCall) {
        // Показываем исходящий экран и пушим уведомление до gUM, чтобы не задерживать вызов
        let modal = document.getElementById('callModal');
        if (!modal) {
            createCallModal();
            modal = document.getElementById('callModal');
        }
        const callUserName = document.getElementById('callUserName');
        const callAvatar = document.getElementById('callAvatar');
        if (callUserName && currentChat) {
            callUserName.textContent = currentChat.name || 'Пользователь';
        }
        if (callAvatar && currentChat) {
            callAvatar.textContent = (currentChat.name || 'U').charAt(0).toUpperCase();
        }
        showCallModal('outgoing', effectiveCallType);
        startCallTimer();
        await sendCallNotification(effectiveCallType);
    }
    
    try {
        cleanupMediaTracks(localStream, 'pre-startCall');
        if (rawLocalStream && rawLocalStream !== localStream) {
            cleanupMediaTracks(rawLocalStream, 'pre-startCall-raw');
        }
        stopLocalAudioProcessing('pre-startCall');
        localStream = null;
        rawLocalStream = null;
        // Получаем список устройств
        try {
            await enumerateDevices();
        } catch (e) {
            console.warn('[Call] enumerateDevices failed, continue without list:', e);
        }
        
        // Запрашиваем доступ к медиа устройствам (автовыбор дефолтного микрофона)
        const audioConstraints = buildAudioEnhancementConstraints(false);
        if (selectedAudioInputId) {
            audioConstraints.deviceId = { exact: selectedAudioInputId };
        }
        const allowGroupVideo = isGroupCall && pendingGroupSfuConfig && effectiveCallType === 'video';
        const constraints = {
            audio: Object.keys(audioConstraints).length ? audioConstraints : true,
            video: (!isGroupCall && effectiveCallType === 'video') || allowGroupVideo
                ? buildCameraVideoConstraintsForCall()
                : false
        };
        
        try {
            rawLocalStream = await getUserMediaWithRetry(constraints);
        } catch (e) {
            console.warn('[Call] gUM failed, falling back to no-media join:', e);
            rawLocalStream = null;
        }
        if (!rawLocalStream) {
            notifications?.warning && notifications.warning('Нет камеры/микрофона — подключаемся без них');
            localStream = new MediaStream(); // пустой поток, только для сигнального оформления
        } else {
            await enhanceLocalAudioStream(rawLocalStream, 'startCall');
            localStream = await buildLocalStreamWithProcessing(rawLocalStream, 'startCall');
        }
        if (!localStream) {
            localStream = new MediaStream();
        }
        // Для групповых без SFU — оставляем только аудио треки
        if (isGroupCall && localStream && !pendingGroupSfuConfig) {
            stripGroupVideoTracks('group-no-sfu');
        }
        
        // Добавляем себя в список участников
        const currentUserId = localStorage.getItem('xipher_user_id');
        if (currentUserId) {
            callParticipants.add(currentUserId);
        }
        
        if (isGroupCall) {
            // Групповой звонок
            await startGroupCall(effectiveCallType);
        } else {
            // Одиночный звонок
            // Убеждаемся, что модальное окно создано
            const modal = document.getElementById('callModal');
            if (!modal) {
                createCallModal();
            }
        
        // Устанавливаем локальный поток в видео элемент
        const localVideo = document.getElementById('localVideo');
        if (localVideo) {
            localVideo.srcObject = localStream;
            // Локальное видео должно быть muted, чтобы избежать эха
            localVideo.muted = true;
        }
        
            // Обновляем имя и аватар в модальном окне
            const callUserName = document.getElementById('callUserName');
            const callAvatar = document.getElementById('callAvatar');
            if (callUserName && currentChat) {
                callUserName.textContent = currentChat.name || 'Пользователь';
            }
            if (callAvatar && currentChat) {
                callAvatar.textContent = (currentChat.name || 'U').charAt(0).toUpperCase();
            }
        
        // Инициализируем peer connection для исходящего звонка
        await initPeerConnection(true);
        
        // Начинаем проверку ответа на звонок
        startCallResponsePolling();
        }
        
    } catch (error) {
        console.error('Ошибка при начале звонка:', error);
        if (error.name === 'NotAllowedError') {
            notifications.error('Доступ к камере/микрофону запрещен');
        } else if (error.name === 'NotFoundError') {
            notifications.error('Камера/микрофон не найдены');
        } else {
            notifications.error('Не удалось начать звонок: ' + error.message);
        }
        endCall();
    }
}

// Начать групповой звонок
async function startGroupCall(callType = 'video') {
    // Получаем ID группы из activeGroup или currentChat
    const activeGroup = window.groupsModule && typeof window.groupsModule.currentGroup === 'function'
        ? window.groupsModule.currentGroup()
        : (window.currentGroup || null);
    const groupId = activeGroup ? activeGroup.id : (currentChat ? currentChat.id : null);
    
    if (!groupId) {
        console.error('[GroupCall] No group ID available');
        notifications.error('Не удалось определить группу для звонка');
        return;
    }
    
    console.log('[GroupCall] Starting group call for group:', groupId);
    
    try {
        // Получаем список участников группы
        const groupMembers = await getGroupMembers(groupId);
        
        if (!groupMembers || groupMembers.length === 0) {
            console.warn('[GroupCall] Не удалось получить участников, используем пустой список');
            // Продолжаем работу даже без участников - они могут присоединиться позже
        }
        
        // Убеждаемся, что модальное окно создано
        const modal = document.getElementById('callModal');
        if (!modal) {
            createCallModal();
        }
        
        // Обновляем имя и аватар в модальном окне
        const callUserName = document.getElementById('callUserName');
        const callAvatar = document.getElementById('callAvatar');
        const groupName = activeGroup ? activeGroup.name : (currentChat ? currentChat.name : 'Группа');
        if (callUserName) {
            callUserName.textContent = groupName;
        }
        if (callAvatar) {
            callAvatar.textContent = (groupName || 'G').charAt(0).toUpperCase();
        }
        
        // Показываем модальное окно группового звонка СРАЗУ
        console.log('[GroupCall] Showing outgoing group call modal');
        showGroupCallModal('outgoing', callType);
        isCallActive = true;
        resetRemoteMediaState();
        startCallTimer();
        
        // Обновляем счетчик участников
        updateParticipantsCount();
        
        // Отображаем список всех участников группы
        renderGroupParticipantsList(groupMembers || [], groupId);
        
        // Отправляем уведомление всем участникам группы (если они есть)
        if (groupMembers && groupMembers.length > 0) {
            try {
                await sendGroupCallNotification(callType, groupMembers, groupId);
            } catch (error) {
                console.error('[GroupCall] Ошибка отправки уведомлений:', error);
                // Продолжаем работу даже если уведомления не отправились
            }

            const hadSfuConfig = !!pendingGroupSfuConfig;
            const sfuStarted = await startGroupCallSfu(groupId, callType, pendingGroupSfuConfig);
            pendingGroupSfuConfig = null;
            if (sfuStarted) {
                console.log('[GroupCall] SFU session started');
                return;
            }
            if (hadSfuConfig) {
                stripGroupVideoTracks('sfu-fallback');
            }
            
            // Инициализируем соединения с каждым участником
            const currentUserId = localStorage.getItem('xipher_user_id');
            for (const member of groupMembers) {
                const memberId = member.user_id || member.id;
                
                // Пропускаем себя
                if (memberId && memberId.toString() === currentUserId) {
                    continue;
                }
                
                // Создаем peer connection для этого участника
                try {
                    await initGroupPeerConnection(memberId, true);
                } catch (error) {
                    console.error('[GroupCall] Ошибка создания соединения с участником', memberId, ':', error);
                    // Продолжаем с другими участниками
                }
            }
        } else {
            // Если участников нет, просто показываем модальное окно
            // Участники могут присоединиться позже через уведомления
            console.log('[GroupCall] Нет участников для подключения, ожидаем присоединения');
            const hadSfuConfig = !!pendingGroupSfuConfig;
            const sfuStarted = await startGroupCallSfu(groupId, callType, pendingGroupSfuConfig);
            pendingGroupSfuConfig = null;
            if (sfuStarted) {
                console.log('[GroupCall] SFU session started (no members)');
            }
            if (!sfuStarted && hadSfuConfig) {
                stripGroupVideoTracks('sfu-fallback');
            }
        }
    } catch (error) {
        console.error('[GroupCall] Критическая ошибка при начале группового звонка:', error);
        notifications.error('Не удалось начать групповой звонок: ' + error.message);
        endCall();
    }
}

// Отправить уведомление о звонке
async function sendCallNotification(callType) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        const response = await fetch(API_BASE + '/api/call-notification', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token: token,
                receiver_id: currentChat.id,
                call_type: callType
            })
        });
        
        const data = await response.json();
        if (!data.success) {
            console.error('Ошибка отправки уведомления о звонке:', data.message);
        }
    } catch (error) {
        console.error('Ошибка отправки уведомления о звонке:', error);
    }
}

// Получить список участников группы
async function getGroupMembers(groupId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) {
        console.warn('[GroupCall] Нет токена для получения участников');
        return [];
    }
    
    try {
        const response = await fetch(API_BASE + '/api/get-group-members', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token: token,
                group_id: groupId
            })
        });
        
        if (!response.ok) {
            console.warn('[GroupCall] API endpoint не найден или недоступен:', response.status);
            // Возвращаем пустой массив, но не прерываем работу
            return [];
        }
        
        const data = await response.json();
        if (data.success && data.members) {
            console.log('[GroupCall] Получено участников:', data.members.length);
            return data.members;
        } else {
            console.warn('[GroupCall] API вернул неожиданный ответ:', data);
        }
        return [];
    } catch (error) {
        console.error('[GroupCall] Ошибка получения участников группы:', error);
        // Возвращаем пустой массив вместо прерывания работы
        return [];
    }
}

// Отправить уведомление о групповом звонке
async function sendGroupCallNotification(callType, members, groupId = null) {
    // Получаем groupId если не передан
    if (!groupId) {
        const activeGroup = window.groupsModule && typeof window.groupsModule.currentGroup === 'function'
            ? window.groupsModule.currentGroup()
            : (window.currentGroup || null);
        groupId = activeGroup ? activeGroup.id : (currentChat ? currentChat.id : null);
    }
    
    if (!groupId) {
        console.error('[GroupCall] No group ID for notification');
        return;
    }
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        const response = await fetch(API_BASE + '/api/group-call-notification', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token: token,
                group_id: groupId,
                call_type: callType,
                members: members.map(m => m.user_id || m.id)
            })
        });
        
        const data = await response.json();
        if (!data.success) {
            console.error('Ошибка отправки уведомления о групповом звонке:', data.message);
        }
    } catch (error) {
        console.error('Ошибка отправки уведомления о групповом звонке:', error);
    }
}

// Добавить видео элемент для участника группового звонка
function addParticipantVideo(userId, stream) {
    const participantsContainer = document.getElementById('groupCallParticipants');
    if (!participantsContainer) {
        console.error('[GroupCall] Контейнер участников не найден');
        return;
    }
    
    // Проверяем, не существует ли уже видео для этого участника
    const existingVideo = document.getElementById(`participant-video-${userId}`);
    if (existingVideo) {
        existingVideo.srcObject = stream;
        return;
    }
    
    // Создаем контейнер для видео участника
    const participantDiv = document.createElement('div');
    participantDiv.className = 'group-call-participant';
    participantDiv.id = `participant-${userId}`;
    
    const video = document.createElement('video');
    video.id = `participant-video-${userId}`;
    video.autoplay = true;
    video.playsinline = true;
    video.srcObject = stream;
    video.muted = document.getElementById('remoteAudio')?.muted ?? false;
    video.volume = Math.min(1, headphoneVolumeLevel);
    
    // Получаем имя участника
    const participantName = getParticipantName(userId);
    const nameDiv = document.createElement('div');
    nameDiv.className = 'participant-name';
    nameDiv.textContent = participantName;
    
    participantDiv.appendChild(video);
    participantDiv.appendChild(nameDiv);
    participantsContainer.appendChild(participantDiv);
    
    // Добавляем участника в список активных
    callParticipants.add(userId.toString());
    updateParticipantsCount();
    updateParticipantsList(); // Обновляем список участников
    
    // Воспроизводим видео
    video.play().catch(err => {
        console.error('[GroupCall] Ошибка воспроизведения видео участника:', err);
    });
}

// Удалить участника из группового звонка
function removeParticipant(userId) {
    callParticipants.delete(userId.toString());
    updateParticipantsCount();
    updateParticipantsList(); // Обновляем список участников

    sfuParticipantLabels.delete(userId);
    
    // Удаляем видео элемент
    const participantDiv = document.getElementById(`participant-${userId}`);
    if (participantDiv) {
        const video = participantDiv.querySelector('video');
        if (video && video.srcObject) {
            video.srcObject.getTracks().forEach(track => track.stop());
        }
        participantDiv.remove();
    }
    
    // Закрываем peer connection
    if (peerConnections.has(userId)) {
        const pc = peerConnections.get(userId);
        if (pc && pc.__recoveryTimer) {
            clearTimeout(pc.__recoveryTimer);
            pc.__recoveryTimer = null;
        }
        pc?.close();
        peerConnections.delete(userId);
    }
    
    // Удаляем поток
    remoteStreams.delete(userId);
}

// Обновить счетчик участников
function updateParticipantsCount() {
    const countElement = document.getElementById('callParticipantsCount');
    if (countElement) {
        const count = callParticipants.size;
        countElement.textContent = `${count} ${count === 1 ? 'участник' : count < 5 ? 'участника' : 'участников'}`;
    }
}

function setCallModalState(type) {
    const modalElement = document.getElementById('callModal');
    if (!modalElement) return;
    modalElement.classList.remove('state-incoming', 'state-outgoing', 'state-active');
    if (type) {
        modalElement.classList.add(`state-${type}`);
        modalElement.dataset.callState = type;
    } else {
        delete modalElement.dataset.callState;
    }
    modalElement.classList.add('is-visible');
}

// Получить имя участника
function getParticipantName(userId) {
    if (sfuParticipantLabels.has(userId)) {
        return sfuParticipantLabels.get(userId);
    }
    // Пытаемся найти в списке друзей
    if (typeof friends !== 'undefined' && friends.length > 0) {
        const friend = friends.find(f => f.id === userId || f.user_id === userId);
        if (friend) {
            return friend.display_name || friend.username || friend.name || 'Участник';
        }
    }
    
    // Пытаемся найти в чатах
    if (typeof chats !== 'undefined' && chats.length > 0) {
        const chat = chats.find(c => c.id === userId || c.user_id === userId);
        if (chat) {
            return chat.display_name || chat.name || 'Участник';
        }
    }
    
    return 'Участник';
}

// Показать модальное окно группового звонка
function showGroupCallModal(type, callType = 'video') {
    const modal = document.getElementById('callModal');
    if (!modal) {
        createCallModal();
    }
    
    const modalElement = document.getElementById('callModal');
    if (!modalElement) {
        console.error('[GroupCall] Не удалось найти модальное окно');
        return;
    }
    setCallModalState(type);
    
    // Показываем контейнер для группового звонка
    const groupCallContainer = document.getElementById('groupCallContainer');
    const singleCallContainer = document.getElementById('singleCallContainer');
    if (groupCallContainer) groupCallContainer.style.display = 'flex';
    if (singleCallContainer) singleCallContainer.style.display = 'none';
    
    const callTypeText = callType === 'video' ? 'Групповой видеозвонок' : 'Групповой звонок';
    // Используем правильный ID для группового звонка
    const statusElement = document.getElementById('groupCallStatus') || document.getElementById('callStatus');
    if (statusElement) {
        if (type === 'outgoing') {
            statusElement.textContent = `Начинаем ${callTypeText.toLowerCase()}...`;
        } else if (type === 'incoming') {
            statusElement.textContent = `Входящий ${callTypeText.toLowerCase()}`;
        } else if (type === 'active') {
            statusElement.textContent = callTypeText;
        }
    }
    
    // Управляем видимостью кнопок для группового звонка
    const outgoingActions = document.getElementById('groupCallActionsOutgoing');
    const incomingActions = document.getElementById('groupCallActionsIncoming');
    const activeActions = document.getElementById('groupCallActionsActive');
    
    // Скрываем кнопки из одиночного звонка
    const singleOutgoing = document.getElementById('cancelCallBtn')?.closest('.call-actions-outgoing') || modalElement.querySelector('.call-actions-outgoing');
    const singleIncoming = document.getElementById('acceptCallBtn')?.closest('.call-actions-incoming') || modalElement.querySelector('.call-actions-incoming');
    const singleActive   = document.getElementById('endCallBtn')?.closest('.call-actions-active')       || modalElement.querySelector('.call-actions-active');
    if (singleOutgoing) singleOutgoing.style.display = 'none';
    if (singleIncoming) singleIncoming.style.display = 'none';
    if (singleActive) singleActive.style.display = 'none';
    
    if (type === 'outgoing') {
        if (outgoingActions) outgoingActions.style.display = 'flex';
        if (incomingActions) incomingActions.style.display = 'none';
        if (activeActions) activeActions.style.display = 'none';
    } else if (type === 'incoming') {
        if (outgoingActions) outgoingActions.style.display = 'none';
        if (incomingActions) incomingActions.style.display = 'flex';
        if (activeActions) activeActions.style.display = 'none';
    } else if (type === 'active') {
        if (outgoingActions) outgoingActions.style.display = 'none';
        if (incomingActions) incomingActions.style.display = 'none';
        if (activeActions) activeActions.style.display = 'flex';
        isCallActive = true;
        resetRemoteMediaState();
    }
    
    // Устанавливаем локальное видео (если уже есть)
    if (localStream) {
        const localVideo = document.getElementById('localVideo');
        if (localVideo) {
            localVideo.srcObject = localStream;
            localVideo.muted = true;
        }
    }
    
    // Обновляем счетчик участников
    updateParticipantsCount();
    
    // Показываем модальное окно
    modalElement.style.display = 'flex';
    modalElement.style.zIndex = '10000';
    
    console.log('[GroupCall] Modal displayed, type:', type, 'callType:', callType);
    console.log('[GroupCall] Incoming actions visible:', incomingActions ? incomingActions.style.display : 'not found');
}

// Показать модальное окно звонка
function showCallModal(type, callType = 'video') {
    // type: 'outgoing', 'incoming', 'active'
    console.log('[Call] showCallModal called:', type, callType, 'isGroupCall:', isGroupCall);
    
    // Если это групповой звонок, используем специальную функцию
    if (isGroupCall) {
        showGroupCallModal(type, callType);
        return;
    }
    
    const modal = document.getElementById('callModal');
    if (!modal) {
        console.log('[Call] Creating call modal');
        createCallModal();
    }
    
    const modalElement = document.getElementById('callModal');
    if (!modalElement) {
        console.error('[Call] Failed to create/find call modal');
        return;
    }
    setCallModalState(type);
    
    // Скрываем контейнер для группового звонка, показываем одиночный
    const groupCallContainer = document.getElementById('groupCallContainer');
    const singleCallContainer = document.getElementById('singleCallContainer');
    if (groupCallContainer) groupCallContainer.style.display = 'none';
    if (singleCallContainer) singleCallContainer.style.display = 'block';

    const displayName = currentChat && currentChat.name ? currentChat.name : 'Пользователь';
    const displayInitial = (displayName || 'U').charAt(0).toUpperCase();
    const avatarUrl = currentChat?.avatar_url || currentChat?.avatarUrl || '';
    
    const callUserName = document.getElementById('callUserName');
    const callAvatar = document.getElementById('callAvatar');
    if (callUserName) {
        callUserName.textContent = displayName;
    }
    if (callAvatar) {
        // Устанавливаем аватар - либо изображение, либо инициал
        const avatarImg = callAvatar.querySelector('img');
        const avatarFallback = callAvatar.querySelector('.avatar-fallback');
        if (avatarUrl && avatarImg) {
            avatarImg.src = avatarUrl;
            avatarImg.style.display = 'block';
            if (avatarFallback) avatarFallback.style.display = 'none';
        } else if (avatarFallback) {
            avatarFallback.textContent = displayInitial;
            avatarFallback.style.display = 'flex';
            if (avatarImg) avatarImg.style.display = 'none';
        }
    }
    const minimizedName = document.querySelector('#minimizedCall .minimized-call-name');
    const minimizedAvatar = document.querySelector('#minimizedCall .minimized-call-avatar');
    if (minimizedName) {
        minimizedName.textContent = displayName;
    }
    if (minimizedAvatar) {
        minimizedAvatar.textContent = displayInitial;
    }
    
    const callTypeText = callType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
    
    // Находим элемент статуса по ID или классу
    const statusElement = document.getElementById('callStatus') || modalElement.querySelector('.call-status');
    
    // Берем элементы кнопок только из одиночного контейнера (чтобы не трогать групповые)
    // Используем реальные блоки кнопок одиночного звонка (вне видео-контейнера)
    const singleOutgoingActions = document.getElementById('cancelCallBtn')?.closest('.call-actions-outgoing') || modalElement.querySelector('.call-actions-outgoing');
    const singleIncomingActions = document.getElementById('acceptCallBtn')?.closest('.call-actions-incoming') || modalElement.querySelector('.call-actions-incoming');
    const singleActiveActions   = document.getElementById('endCallBtn')?.closest('.call-actions-active')     || modalElement.querySelector('.call-actions-active');
    
    if (type === 'outgoing') {
        if (statusElement) {
            statusElement.textContent = `Исходящий ${callTypeText}...`;
        }
        if (singleOutgoingActions) singleOutgoingActions.style.display = 'flex';
        if (singleIncomingActions) singleIncomingActions.style.display = 'none';
        if (singleActiveActions)   singleActiveActions.style.display   = 'none';
        // Скрываем fallback панели - основной UI уже fullscreen
        hideCallControlsBar();
        hideInlineCallPrompt();
    } else if (type === 'incoming') {
        console.log('[Call] Showing incoming call modal');
        if (statusElement) {
            statusElement.textContent = `Входящий ${callTypeText}`;
        }
        if (singleOutgoingActions) singleOutgoingActions.style.display = 'none';
        if (singleIncomingActions) {
            singleIncomingActions.style.display = 'flex';
            console.log('[Call] Incoming actions displayed');
        } else {
            console.error('[Call] Incoming actions element not found!');
        }
        if (singleActiveActions) singleActiveActions.style.display = 'none';
        // Скрываем fallback панели
        hideCallControlsBar();
        hideInlineCallPrompt();
    } else if (type === 'active') {
        if (statusElement) {
            statusElement.textContent = callTypeText;
        }
        if (singleOutgoingActions) singleOutgoingActions.style.display = 'none';
        if (singleIncomingActions) singleIncomingActions.style.display = 'none';
        if (singleActiveActions)   singleActiveActions.style.display   = 'flex';
        isCallActive = true;
        // Скрываем fallback панели - fullscreen UI уже имеет все кнопки
        hideCallControlsBar();
        hideInlineCallPrompt();
    }
    
    modalElement.style.display = 'flex';
    modalElement.style.zIndex = '10000';
    console.log('[Call] Modal displayed with z-index 10000');
    
    // Обновляем плейсхолдер аватара
    const placeholderAvatar = document.getElementById('placeholderAvatar');
    const placeholderAvatarImg = document.getElementById('placeholderAvatarImg');
    const placeholderInitial = document.getElementById('placeholderInitial');
    const placeholderName = document.getElementById('placeholderName');
    const placeholderStatus = document.getElementById('placeholderStatus');
    
    if (placeholderAvatar) {
        if (avatarUrl && placeholderAvatarImg) {
            placeholderAvatarImg.src = avatarUrl;
            placeholderAvatarImg.style.display = 'block';
            if (placeholderInitial) placeholderInitial.style.display = 'none';
        } else if (placeholderInitial) {
            placeholderInitial.textContent = displayInitial;
            placeholderInitial.style.display = 'flex';
            if (placeholderAvatarImg) placeholderAvatarImg.style.display = 'none';
        }
    }
    if (placeholderName) {
        placeholderName.textContent = displayName;
    }
    if (placeholderStatus) {
        const callTypeText = callType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
        if (type === 'incoming') {
            placeholderStatus.textContent = `Входящий ${callTypeText.toLowerCase()}`;
        } else if (type === 'outgoing') {
            placeholderStatus.textContent = `Исходящий ${callTypeText.toLowerCase()}...`;
        } else {
            placeholderStatus.textContent = callTypeText;
        }
    }
    
    // Показываем/скрываем плейсхолдер в зависимости от типа звонка
    const videoPlaceholder = document.getElementById('callVideoPlaceholder');
    if (videoPlaceholder) {
        if (callType === 'audio' || type === 'outgoing' || type === 'incoming') {
            videoPlaceholder.classList.remove('hidden');
        }
    }
    
    // Обновляем бейдж качества
    const quality = getCallQualitySettingForCall('camera');
    const qualityBadge = document.getElementById('callQualityBadge');
    if (qualityBadge && quality) {
        qualityBadge.textContent = quality.resolution >= 1080 ? 'FHD' : quality.resolution >= 720 ? 'HD' : 'SD';
    }
    
    // Устанавливаем локальное видео, если есть
    if (localStream) {
        const localVideo = document.getElementById('localVideo');
        if (localVideo) {
            localVideo.srcObject = localStream;
            // Локальное видео должно быть muted, чтобы избежать эха
            localVideo.muted = true;
        }
    }
}

// Создать модальное окно звонка — Fullscreen Telegram/Discord style
function createCallModal() {
    const modal = document.createElement('div');
    modal.id = 'callModal';
    modal.className = 'call-modal-overlay';
    const displayName = currentChat?.name || 'Пользователь';
    const displayInitial = displayName.charAt(0).toUpperCase();
    const avatarUrl = currentChat?.avatar_url || currentChat?.avatarUrl || '';
    modal.innerHTML = `
        <div class="call-modal-content" id="callModalContent">
            <!-- Хедер -->
            <header class="call-header">
                <div class="call-user-info">
                    <div class="call-avatar" id="callAvatar">
                        ${avatarUrl ? `<img src="${avatarUrl}" alt="${displayName}" onerror="this.style.display='none';this.nextElementSibling.style.display='flex'"><span class="avatar-fallback" style="display:none">${displayInitial}</span>` : `<span class="avatar-fallback">${displayInitial}</span>`}
                    </div>
                    <div class="call-user-details">
                        <h3 id="callUserName">${displayName}</h3>
                        <p class="call-status" id="callStatus">Подключение...</p>
                    </div>
                </div>
                <p class="call-timer" id="callTimer">00:00</p>
                <div class="call-header-actions">
                    <button class="call-btn-minimize" id="minimizeCallBtn" title="Свернуть">${callIcon('minimize')}</button>
                    <button class="call-btn-settings" id="callSettingsBtn" title="Настройки">${callIcon('settings')}</button>
                </div>
            </header>
            
            <!-- Меню настроек (Discord style) -->
            <div class="call-settings-menu" id="callSettingsMenu">
                <div class="call-setting-section">
                    <div class="call-setting-title">${callIcon('video')} Качество видео</div>
                    <div class="call-quality-presets" id="callQualityPresets">
                        <div class="call-quality-preset" data-resolution="360" data-fps="30">
                            <div class="preset-resolution">360p</div>
                            <div class="preset-label">Эконом</div>
                            <div class="preset-fps">30 FPS</div>
                        </div>
                        <div class="call-quality-preset" data-resolution="720" data-fps="30">
                            <div class="preset-resolution">720p</div>
                            <div class="preset-label">HD</div>
                            <div class="preset-fps">30 FPS</div>
                        </div>
                        <div class="call-quality-preset active" data-resolution="1080" data-fps="60">
                            <div class="preset-resolution">1080p</div>
                            <div class="preset-label">Full HD</div>
                            <div class="preset-fps">60 FPS</div>
                            <div class="preset-badge">${callIcon('premium')} Premium</div>
                        </div>
                    </div>
                </div>
                
                <div class="call-setting-section">
                    <div class="call-setting-title">${callIcon('headphones')} Устройства</div>
                    <div class="call-setting-item">
                        <label>${callIcon('mic')} Микрофон</label>
                        <select id="audioInputSelect" class="call-device-select">
                            <option value="">По умолчанию</option>
                        </select>
                    </div>
                    <div class="call-setting-item">
                        <label>${callIcon('speaker')} Динамики</label>
                        <select id="audioOutputSelect" class="call-device-select">
                            <option value="">По умолчанию</option>
                        </select>
                    </div>
                    <div class="call-setting-item">
                        <label>${callIcon('video')} Камера</label>
                        <select id="videoInputSelect" class="call-device-select">
                            <option value="">По умолчанию</option>
                        </select>
                    </div>
                </div>
                
                <div class="call-setting-section">
                    <div class="call-setting-title">${callIcon('speaker')} Громкость</div>
                    <div class="call-setting-item">
                        <label>Микрофон</label>
                        <div class="call-volume-control">
                            <span class="call-volume-icon">${callIcon('mic')}</span>
                            <input type="range" class="call-volume-slider" id="micVolumeSlider" min="0" max="150" value="100">
                            <span class="call-volume-value" id="micVolumeValue">100%</span>
                        </div>
                    </div>
                    <div class="call-setting-item">
                        <label>Наушники</label>
                        <div class="call-volume-control">
                            <span class="call-volume-icon">${callIcon('headphones')}</span>
                            <input type="range" class="call-volume-slider" id="headphoneVolumeSlider" min="0" max="150" value="100">
                            <span class="call-volume-value" id="headphoneVolumeValue">100%</span>
                        </div>
                    </div>
                </div>
                
                <div class="call-setting-section">
                    <div class="call-setting-title">${callIcon('settings')} Детальные настройки</div>
                    <div class="call-setting-item">
                        <label>Камера (ручной выбор)</label>
                        <div class="call-quality-inline">
                            <select id="callCameraQualitySelectInline" class="call-device-select">
                                <option value="360">360p</option>
                                <option value="540">540p</option>
                                <option value="720">720p</option>
                                <option value="1080" data-premium="true">1080p</option>
                            </select>
                            <select id="callCameraFpsSelectInline" class="call-device-select">
                                <option value="15">15 FPS</option>
                                <option value="30" data-premium="true">30 FPS</option>
                                <option value="60" data-premium="true">60 FPS</option>
                            </select>
                        </div>
                    </div>
                    <div class="call-setting-item">
                        <label>Демонстрация экрана</label>
                        <div class="call-quality-inline">
                            <select id="callScreenQualitySelectInline" class="call-device-select">
                                <option value="360">360p</option>
                                <option value="540">540p</option>
                                <option value="720">720p</option>
                                <option value="1080" data-premium="true">1080p</option>
                            </select>
                            <select id="callScreenFpsSelectInline" class="call-device-select">
                                <option value="15">15 FPS</option>
                                <option value="30" data-premium="true">30 FPS</option>
                                <option value="60" data-premium="true">60 FPS</option>
                            </select>
                        </div>
                    </div>
                    <div class="call-quality-note" id="callQualityPremiumNoteInline">1080p и 60 FPS доступны в Premium</div>
                </div>
            </div>
            
            <!-- Основная область видео -->
            <div class="call-video-container" id="singleCallContainer">
                <video id="remoteVideo" autoplay playsinline></video>
                <video id="localVideo" autoplay muted playsinline></video>
                
                <!-- Информационная панель сверху -->
                <div class="call-info-bar" id="callInfoBar">
                    <div class="call-info-item">
                        <span class="call-timer" id="callTimerOverlay">00:00</span>
                    </div>
                    <div class="call-info-item">
                        <div class="call-quality-bars" id="callQualityBars">
                            <div class="call-quality-bar"></div>
                            <div class="call-quality-bar"></div>
                            <div class="call-quality-bar"></div>
                            <div class="call-quality-bar"></div>
                        </div>
                        <span class="call-quality-badge" id="callQualityBadge">HD</span>
                    </div>
                    <div class="call-info-item">
                        ${callIcon('lock')}
                        <span>E2E</span>
                    </div>
                </div>
                
                <!-- Плейсхолдер когда нет видео -->
                <div class="call-video-placeholder" id="callVideoPlaceholder">
                    <div class="placeholder-avatar" id="placeholderAvatar">
                        ${avatarUrl ? `<img id="placeholderAvatarImg" src="${avatarUrl}" alt="${displayName}" onerror="this.style.display='none';document.getElementById('placeholderInitial').style.display='flex'">` : `<img id="placeholderAvatarImg" src="" alt="" style="display:none">`}
                        <span class="placeholder-initial" id="placeholderInitial" ${avatarUrl ? 'style="display:none"' : ''}>${displayInitial}</span>
                    </div>
                    <div class="placeholder-name" id="placeholderName">${displayName}</div>
                    <div class="placeholder-status" id="placeholderStatus">Голосовой звонок</div>
                </div>
            </div>
            
            <!-- Групповой звонок -->
            <div class="group-call-container" id="groupCallContainer" style="display: none;">
                <div class="group-call-header">
                    <p class="call-status" id="groupCallStatus">Групповой звонок</p>
                    <p class="call-participants-count" id="callParticipantsCount">1 участник</p>
                    <p class="call-timer" id="groupCallTimer">00:00</p>
                </div>
                <div class="group-call-participants" id="groupCallParticipants"></div>
                <div class="group-call-local-video">
                    <video id="groupLocalVideo" autoplay muted playsinline></video>
                    <div class="local-video-label">Вы</div>
                </div>
                <div class="call-actions-outgoing" id="groupCallActionsOutgoing" style="display: none;">
                    <button class="call-btn call-btn-cancel" id="cancelGroupCallBtn">
                        <span class="call-btn-icon">${callIcon('hangup')}</span>
                        <span class="call-btn-label">Отменить</span>
                    </button>
                </div>
                <div class="call-actions-incoming" id="groupCallActionsIncoming" style="display: none;">
                    <button class="call-btn call-btn-accept" id="acceptGroupCallBtn">
                        <span class="call-btn-icon">${callIcon('phone')}</span>
                        <span class="call-btn-label">Принять</span>
                    </button>
                    <button class="call-btn call-btn-reject" id="rejectGroupCallBtn">
                        <span class="call-btn-icon">${callIcon('hangup')}</span>
                        <span class="call-btn-label">Отклонить</span>
                    </button>
                </div>
                <div class="call-actions-active" id="groupCallActionsActive" style="display: none;">
                    <button class="call-btn call-btn-toggle" id="toggleMicBtnGroup" title="Микрофон">${callIcon('mic')}</button>
                    <button class="call-btn call-btn-toggle" id="toggleVideoBtnGroup" title="Камера">${callIcon('video')}</button>
                    <button class="call-btn call-btn-toggle" id="toggleScreenBtnGroup" title="Демонстрация экрана">${callIcon('screen')}</button>
                    <button class="call-btn call-btn-toggle" id="toggleAudioBtnGroup" title="Звук">${callIcon('speaker')}</button>
                    <button class="call-btn call-btn-end" id="endGroupCallBtn">
                        <span class="call-btn-icon">${callIcon('hangup')}</span>
                    </button>
                </div>
            </div>
            
            <!-- Панель управления (Discord style) -->
            <div class="call-actions-outgoing">
                <button class="call-btn call-btn-cancel" id="cancelCallBtn">
                    <span class="call-btn-icon">${callIcon('hangup')}</span>
                    <span class="call-btn-label">Отменить</span>
                </button>
            </div>
            
            <div class="call-actions-incoming">
                <button class="call-btn call-btn-accept" id="acceptCallBtn">
                    <span class="call-btn-icon">${callIcon('phone')}</span>
                    <span class="call-btn-label">Принять</span>
                </button>
                <button class="call-btn call-btn-reject" id="rejectCallBtn">
                    <span class="call-btn-icon">${callIcon('hangup')}</span>
                    <span class="call-btn-label">Отклонить</span>
                </button>
            </div>
            
            <div class="call-actions-active">
                <button class="call-btn call-btn-toggle" id="toggleMicBtn" title="Микрофон">${callIcon('mic')}</button>
                <button class="call-btn call-btn-toggle" id="toggleVideoBtn" title="Камера">${callIcon('video')}</button>
                <button class="call-btn call-btn-toggle" id="toggleScreenBtn" title="Демонстрация экрана">${callIcon('screen')}</button>
                <div class="call-controls-divider"></div>
                <button class="call-btn call-btn-toggle" id="toggleNoiseBtn" title="Шумоподавление">${callIcon('noise')}</button>
                <button class="call-btn call-btn-toggle" id="toggleAudioBtn" title="Звук">${callIcon('speaker')}</button>
                <div class="call-controls-divider"></div>
                <button class="call-btn call-btn-end" id="endCallBtn">
                    <span class="call-btn-icon">${callIcon('hangup')}</span>
                </button>
            </div>
        </div>
    `;
    
    // Создаем минимизированное окно звонка
    const minimizedCall = document.createElement('div');
    minimizedCall.id = 'minimizedCall';
    minimizedCall.className = 'minimized-call';
    minimizedCall.style.display = 'none';
    minimizedCall.innerHTML = `
        <div class="minimized-call-content">
            <div class="minimized-call-info">
                <div class="minimized-call-avatar">${currentChat ? currentChat.name.charAt(0).toUpperCase() : 'U'}</div>
                <div class="minimized-call-details">
                    <div class="minimized-call-name">${currentChat ? currentChat.name : 'Пользователь'}</div>
                    <div class="minimized-call-timer" id="minimizedCallTimer">00:00</div>
                </div>
            </div>
            <div class="minimized-call-actions">
                <button class="minimized-call-btn" id="restoreCallBtn" title="Восстановить">${callIcon('maximize')}</button>
                <button class="minimized-call-btn minimized-call-end" id="endCallMinimizedBtn" title="Завершить">${callIcon('close')}</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    document.body.appendChild(minimizedCall);
    
    // Скрываем fallback панели управления (новый fullscreen UI их полностью заменяет)
    hideCallControlsBar();
    hideInlineCallPrompt();
    
    // Заполняем списки устройств
    populateDeviceSelects();
    
    // Обработчики событий
    document.getElementById('cancelCallBtn')?.addEventListener('click', cancelCall);
    document.getElementById('acceptCallBtn')?.addEventListener('click', acceptCall);
    document.getElementById('rejectCallBtn')?.addEventListener('click', rejectCall);
    document.getElementById('endCallBtn')?.addEventListener('click', endCall);
    document.getElementById('endCallMinimizedBtn')?.addEventListener('click', endCall);
    
    // Обработчики для групповых звонков
    document.getElementById('cancelGroupCallBtn')?.addEventListener('click', cancelCall);
    document.getElementById('acceptGroupCallBtn')?.addEventListener('click', acceptCall);
    document.getElementById('rejectGroupCallBtn')?.addEventListener('click', rejectCall);
    document.getElementById('endGroupCallBtn')?.addEventListener('click', endCall);
    document.getElementById('toggleMicBtn')?.addEventListener('click', toggleMicrophone);
    document.getElementById('toggleMicBtnGroup')?.addEventListener('click', toggleMicrophone);
    document.getElementById('toggleVideoBtn')?.addEventListener('click', toggleVideo);
    document.getElementById('toggleVideoBtnGroup')?.addEventListener('click', toggleVideo);
    document.getElementById('toggleScreenBtn')?.addEventListener('click', toggleScreenShare);
    document.getElementById('toggleScreenBtnGroup')?.addEventListener('click', toggleScreenShare);
    document.getElementById('toggleAudioBtn')?.addEventListener('click', toggleAudio);
    document.getElementById('toggleAudioBtnGroup')?.addEventListener('click', toggleAudio);
    document.getElementById('minimizeCallBtn')?.addEventListener('click', minimizeCall);
    document.getElementById('restoreCallBtn')?.addEventListener('click', restoreCall);
    document.getElementById('callSettingsBtn')?.addEventListener('click', toggleCallSettings);
    document.getElementById('audioInputSelect')?.addEventListener('change', (e) => {
        if (e.target.value) {
            selectAudioInput(e.target.value);
        }
    });
    document.getElementById('audioOutputSelect')?.addEventListener('change', (e) => {
        if (e.target.value) {
            selectAudioOutput(e.target.value);
        }
    });
    document.getElementById('micVolumeSlider')?.addEventListener('input', (e) => {
        setMicVolumePercent(e.target.value);
    });
    document.getElementById('headphoneVolumeSlider')?.addEventListener('input', (e) => {
        setHeadphoneVolumePercent(e.target.value);
    });
    syncVolumeSliders();
    
    // Обработчики для пресетов качества (Discord style)
    document.querySelectorAll('.call-quality-preset').forEach(preset => {
        preset.addEventListener('click', () => {
            document.querySelectorAll('.call-quality-preset').forEach(p => p.classList.remove('active'));
            preset.classList.add('active');
            const resolution = parseInt(preset.dataset.resolution, 10);
            const fps = parseInt(preset.dataset.fps, 10);
            writeCallQualitySettingForCall('camera', { resolution, fps });
            updateInlineCallQualityControls();
            const badge = document.getElementById('callQualityBadge');
            if (badge) badge.textContent = resolution >= 1080 ? 'FHD' : resolution >= 720 ? 'HD' : 'SD';
        });
    });
    
    // Шумоподавление
    document.getElementById('toggleNoiseBtn')?.addEventListener('click', () => {
        const btn = document.getElementById('toggleNoiseBtn');
        if (btn) {
            btn.classList.toggle('active');
        }
    });
    
    // Синхронизация таймера в overlay
    setInterval(() => {
        const mainTimer = document.getElementById('callTimer');
        const overlayTimer = document.getElementById('callTimerOverlay');
        if (mainTimer && overlayTimer && mainTimer.textContent) {
            overlayTimer.textContent = mainTimer.textContent;
        }
    }, 500);
    
    // Скрытие плейсхолдера при получении видео
    const remoteVideo = document.getElementById('remoteVideo');
    if (remoteVideo) {
        remoteVideo.addEventListener('playing', () => {
            const placeholder = document.getElementById('callVideoPlaceholder');
            if (placeholder) placeholder.classList.add('hidden');
        });
    }
}

// Принять звонок с гарантией надёжности
async function acceptCall() {
    // Останавливаем звук звонка
    stopCallRingtone();
    
    if (!initWebRTC()) {
        return;
    }
    
    // Очищаем буфер ICE и загружаем TURN
    pendingIceCandidates = [];
    pendingIceCandidatesWithTs = [];
    
    try {
        await loadTurnConfigOnce();
    } catch (e) {
        console.warn('[Call] TURN load failed on accept, proceeding with STUN:', e);
    }
    
    // Проверяем ICE конфигурацию
    const iceServers = buildIceServers();
    console.log('[Call] ICE servers for accept:', iceServers.length);
    
    isCaller = false;
    pendingGroupSfuConfig = null;
    if (isGroupCall && currentChat && currentChat.id) {
        pendingGroupSfuConfig = await fetchSfuRoomConfig(currentChat.id);
    }
    
    try {
        cleanupMediaTracks(localStream, 'pre-acceptCall');
        if (rawLocalStream && rawLocalStream !== localStream) {
            cleanupMediaTracks(rawLocalStream, 'pre-acceptCall-raw');
        }
        stopLocalAudioProcessing('pre-acceptCall');
        localStream = null;
        rawLocalStream = null;
        try {
            await enumerateDevices();
        } catch (e) {
            console.warn('[Call] enumerateDevices failed on accept, continue:', e);
        }
        
        const audioConstraints = buildAudioEnhancementConstraints(false);
        if (selectedAudioInputId) {
            audioConstraints.deviceId = { exact: selectedAudioInputId };
        }
        const allowGroupVideo = isGroupCall && pendingGroupSfuConfig && currentCallType === 'video';
        const constraints = {
            audio: Object.keys(audioConstraints).length ? audioConstraints : true,
            video: (!isGroupCall && currentCallType === 'video') || allowGroupVideo
                ? buildCameraVideoConstraintsForCall()
                : false
        };
        
        try {
            rawLocalStream = await getUserMediaWithRetry(constraints);
        } catch (e) {
            console.warn('[Call] gUM failed on accept, using empty stream:', e);
            rawLocalStream = null;
        }
        if (!rawLocalStream) {
            notifications?.warning && notifications.warning('Нет камеры/микрофона — подключаемся без них');
            localStream = new MediaStream();
        } else {
            await enhanceLocalAudioStream(rawLocalStream, 'acceptCall');
            localStream = await buildLocalStreamWithProcessing(rawLocalStream, 'acceptCall');
        }
        if (!localStream) {
            localStream = new MediaStream();
        }
        if (isGroupCall && localStream && !pendingGroupSfuConfig) {
            stripGroupVideoTracks('group-no-sfu');
        }
        
        // Добавляем себя в список участников
        const currentUserId = localStorage.getItem('xipher_user_id');
        if (currentUserId) {
            callParticipants.add(currentUserId);
        }
        
        // Разрешаем воспроизведение входящего звука (жест пользователя)
        primeRemoteAudioFromUserGesture();
        
        if (isGroupCall) {
            // Групповой звонок
            console.log('[GroupCall] Accepting group call');
            
            // Устанавливаем локальное видео
        const localVideo = document.getElementById('localVideo');
        if (localVideo && localStream) {
            localVideo.srcObject = localStream;
            localVideo.muted = true;
        }
        
            // Получаем список участников группы
            const groupMembers = await getGroupMembers(currentChat.id);
            
            // Убеждаемся, что модальное окно создано
            const modal = document.getElementById('callModal');
            if (!modal) {
                createCallModal();
            }
            
            // Показываем активный групповой звонок
            showGroupCallModal('active', currentCallType);
            isCallActive = true;
            startCallTimer();
            updateParticipantsCount();

            const hadSfuConfig = !!pendingGroupSfuConfig;
            const sfuStarted = await startGroupCallSfu(currentChat.id, currentCallType, pendingGroupSfuConfig);
            pendingGroupSfuConfig = null;
            if (sfuStarted) {
                console.log('[GroupCall] SFU session started on accept');
                window.pendingGroupCallNotification = null;
                return;
            }
            if (hadSfuConfig) {
                stripGroupVideoTracks('sfu-fallback');
            }
            
            // Инициализируем соединения с каждым участником
            for (const member of groupMembers) {
                const memberId = member.user_id || member.id;
                if (memberId.toString() === currentUserId) {
                    continue;
                }
                
                // Создаем peer connection
            try {
                    await initGroupPeerConnection(memberId, true);
            } catch (error) {
                    console.error('[GroupCall] Ошибка создания соединения с участником', memberId, ':', error);
            }
        }
        
            // Очищаем сохраненное уведомление
            window.pendingGroupCallNotification = null;
        } else {
            // Одиночный звонок - создаем peer connection и обрабатываем offer
            if (!window.pendingCallOffer || !window.pendingCallOffer.offer) {
                // Пытаемся взять последний зафиксированный offer (на случай гонок парсинга)
                if (lastIncomingOffer && lastIncomingOffer.offer) {
                    window.pendingCallOffer = lastIncomingOffer;
                    console.warn('[Call] Recovered pendingCallOffer from lastIncomingOffer');
                }
            }
            if (!window.pendingCallOffer || !window.pendingCallOffer.offer) {
                console.error('[Call] No pending offer to accept');
                notifications.error('Нет данных о звонке для принятия');
                return;
            }
            
            const pendingOffer = window.pendingCallOffer;
            // Если offer остался строкой — пытаемся санитизировать и распарсить
            if (typeof pendingOffer.offer === 'string') {
                try {
                    const cleaned = sanitizeJsonString(pendingOffer.offer);
                    const parsed = safeJsonParse(cleaned);
                    if (parsed) {
                        pendingOffer.offer = parsed;
                    } else if (looksLikeSdp(pendingOffer.offer)) {
                        pendingOffer.offer = { type: 'offer', sdp: pendingOffer.offer };
                    } else {
                        throw new Error('Parsed offer is empty');
                    }
                } catch (e) {
                    console.error('[Call] Failed to parse pending offer in acceptCall:', e);
                    notifications.error('Повреждённый offer, звонок отклонён');
                    rejectCall();
                    return;
                }
            }
            
            // Закрываем существующий peerConnection если он есть
            if (peerConnection) {
                console.warn('[Call] Closing existing peerConnection before accepting new call');
                try {
                    peerConnection.onicecandidate = null;
                    peerConnection.ontrack = null;
                    peerConnection.onconnectionstatechange = null;
                    peerConnection.oniceconnectionstatechange = null;
                    peerConnection.onicegatheringstatechange = null;
                    peerConnection.close();
                } catch (e) {
                    console.warn('[Call] Error closing old peerConnection:', e);
                }
                peerConnection = null;
            }
            
            // Создаем конфигурацию для peer connection
            const configuration = {
                iceServers: buildIceServers(),
                iceCandidatePoolSize: 10
            };
            
    // Создаем peer connection
    peerConnection = new RTCPeerConnection(configuration);
    pendingIceCandidates = [];
            
            // Добавляем локальные треки
            if (localStream && localStream.getTracks().length > 0) {
                localStream.getTracks().forEach(track => {
                    peerConnection.addTrack(track, localStream);
                });
            }
            if (localStream && localStream.getVideoTracks().length > 0) {
                const videoSender = findVideoSender(peerConnection);
                await applyCameraEncodingForCall(videoSender);
            }
            
            // Обработка удаленного потока
            peerConnection.ontrack = (event) => {
                console.log('[Call] Received remote track:', event);
                console.log('[Call] Track kind:', event.track.kind);
                console.log('[Call] Track enabled:', event.track.enabled);
                console.log('[Call] Streams:', event.streams);
                
                const stream = resolveRemoteStreamFromTrack(event);
                remoteStream = stream;
                
                // FIX: Ждем разблокировки трека и принудительно крепим поток
                event.track.onunmute = () => attachRemoteStream(stream);
                attachRemoteStream(stream);
                if (event.track.kind === 'video' && isScreenTrack(event.track)) {
                    remoteScreenShareActive = true;
                    remoteScreenShareTrackId = event.track.id || 'remote-screen';
                    event.track.onended = () => {
                        if (remoteScreenShareTrackId === (event.track.id || 'remote-screen')) {
                            remoteScreenShareActive = false;
                            remoteScreenShareTrackId = null;
                            remoteScreenShareNotified = false;
                        }
                    };
                    if (screenStream) {
                        notifications?.warning && notifications.warning('Собеседник начал демонстрацию экрана — остановили вашу, чтобы не перегружать звонок.');
                        toggleScreenShare();
                    }
                    notifyRemoteScreenShare();
                }
                ensureRemoteFullscreenButton();
                startConnectionWatchdog(peerConnection, 'single');
                
                // Убеждаемся, что модальное окно видно
                const modal = document.getElementById('callModal');
                if (modal && modal.style.display === 'none') {
                    modal.style.display = 'flex';
                }
                
                // Обновляем статус на "Принят"
                const statusElement = document.getElementById('callStatus');
                if (statusElement) {
                    const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
                    statusElement.textContent = callTypeText;
                }
            };
            
            // Обработка ICE кандидатов с буферизацией
            peerConnection.onicecandidate = (event) => {
                if (event.candidate) {
                    console.log('[Call] Sending ICE candidate:', event.candidate.candidate?.substring(0, 50));
                    sendIceCandidate(event.candidate);
                } else {
                    console.log('[Call] ICE gathering complete');
                }
            };
            
            // Отслеживание состояния ICE gathering
            peerConnection.onicegatheringstatechange = () => {
                console.log('[Call] ICE gathering state:', peerConnection.iceGatheringState);
            };
            
            // Обработка состояний соединения с улучшенным восстановлением
            peerConnection.onconnectionstatechange = () => {
                const state = peerConnection.connectionState;
                console.log('[Call] Connection state:', state);
                
                if (state === 'connected') {
                    console.log('[Call] ✅ Connection established successfully');
                    clearCallRecoveryState('connected');
                    const statusElement = document.getElementById('callStatus');
                    if (statusElement) {
                        const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
                        statusElement.textContent = callTypeText;
                    }
                    startAudioWatchdog();
                    attachRemoteStream(remoteStream);
                    startConnectionWatchdog(peerConnection, 'single');
                    // Обработка буферизованных ICE
                    processBufferedIceCandidates();
                } else if (state === 'connecting') {
                    console.log('[Call] ⏳ Connecting...');
                } else if (state === 'failed') {
                    console.warn('[Call] ❌ Connection failed - attempting recovery');
                    beginCallRecovery(peerConnection, 'conn-failed');
                    restartIceSafe(peerConnection);
                    setTimeout(forceRemotePlayback, 150);
                } else if (state === 'disconnected') {
                    console.warn('[Call] ⚠️ Connection disconnected - waiting for recovery');
                    // Даём больше времени на автовосстановление
                    setTimeout(() => {
                        if (peerConnection && peerConnection.connectionState === 'disconnected') {
                            beginCallRecovery(peerConnection, 'conn-disconnected');
                            restartIceSafe(peerConnection);
                        }
                    }, 2000);
                }
            };

            peerConnection.oniceconnectionstatechange = () => {
                const state = peerConnection.iceConnectionState;
                console.log('[Call] ICE connection state:', state);
                
                if (state === 'checking') {
                    console.log('[Call] ⏳ ICE checking connectivity...');
                } else if (state === 'connected' || state === 'completed') {
                    console.log('[Call] ✅ ICE connected');
                    clearCallRecoveryState('ice-connected');
                    startConnectionWatchdog(peerConnection, 'single');
                    processBufferedIceCandidates();
                } else if (state === 'failed') {
                    console.warn('[Call] ❌ ICE failed - restarting');
                    beginCallRecovery(peerConnection, 'ice-failed');
                    restartIceSafe(peerConnection);
                    setTimeout(forceRemotePlayback, 150);
                } else if (state === 'disconnected') {
                    console.warn('[Call] ⚠️ ICE disconnected - waiting');
                    // ICE disconnected часто временное состояние
                    setTimeout(() => {
                        if (peerConnection && peerConnection.iceConnectionState === 'disconnected') {
                            console.warn('[Call] ICE still disconnected - attempting restart');
                            restartIceSafe(peerConnection);
                        }
                    }, 3000);
                }
            };
            
            // Устанавливаем локальное видео
            const localVideo = document.getElementById('localVideo');
            if (localVideo && localStream) {
                localVideo.srcObject = localStream;
                localVideo.muted = true;
            }
            
            // Обрабатываем offer
            try {
                // Проверяем состояние peerConnection перед установкой remote description
                const signalingState = peerConnection.signalingState;
                console.log('[Call] Signaling state before setRemoteDescription:', signalingState);
                
                if (signalingState !== 'stable') {
                    console.warn('[Call] Unexpected signaling state, resetting peerConnection');
                    // Закрываем и пересоздаём
                    peerConnection.close();
                    const configuration = {
                        iceServers: buildIceServers(),
                        iceCandidatePoolSize: 10
                    };
                    peerConnection = new RTCPeerConnection(configuration);
                    pendingIceCandidates = [];
                    
                    // Добавляем локальные треки заново
                    if (localStream && localStream.getTracks().length > 0) {
                        localStream.getTracks().forEach(track => {
                            peerConnection.addTrack(track, localStream);
                        });
                    }
                }
                
        await peerConnection.setRemoteDescription(new RTCSessionDescription(pendingOffer.offer));
                console.log('[Call] Remote description set, signaling state:', peerConnection.signalingState);

                const videoTransceiver = findVideoTransceiver(peerConnection);
                if (videoTransceiver && videoTransceiver.direction !== 'sendrecv') {
                    videoTransceiver.direction = 'sendrecv';
                }
                
                // Проверяем состояние перед созданием answer
                if (peerConnection.signalingState !== 'have-remote-offer') {
                    console.error('[Call] Cannot create answer, signaling state:', peerConnection.signalingState);
                    throw new Error('Invalid signaling state for creating answer: ' + peerConnection.signalingState);
                }
                
                // Создаем answer
                const rawAnswer = await peerConnection.createAnswer();
                const filteredAnswer = {
                    type: rawAnswer.type,
                    sdp: stripCodecs(rawAnswer.sdp || '')
                };
                await peerConnection.setLocalDescription(filteredAnswer);
                console.log('[Call] Answer created (filtered) and local description set');
                
                // Отправляем answer через WebSocket
                await sendAnswer(filteredAnswer);
                console.log('[Call] Answer sent');
                
                // Убеждаемся, что модальное окно создано
                const modal = document.getElementById('callModal');
                if (!modal) {
                    createCallModal();
                }
        
                // Показываем активный звонок СРАЗУ
        showCallModal('active', currentCallType);
        isCallActive = true;
        resetRemoteMediaState();
        
        // Запускаем таймер
        startCallTimer();
        
                // Отправляем начальное состояние медиа собеседнику
                setTimeout(() => sendInitialMediaState(), 500);
        
                // Отправляем ответ о принятии звонка ПОСЛЕ показа UI
                await sendCallResponse('accepted');
        
        // Очищаем сохраненный offer
        window.pendingCallOffer = null;
            } catch (error) {
                console.error('[Call] Error handling offer:', error);
                notifications.error('Ошибка при принятии звонка: ' + error.message);
                rejectCall();
                return;
            }
        }
        
    } catch (error) {
        console.error('Ошибка при принятии звонка:', error);
        notifications.error('Не удалось принять звонок');
        rejectCall();
    }
}

// Отклонить звонок
async function rejectCall() {
    // Останавливаем звук звонка
    stopCallRingtone();
    
    await sendCallResponse('rejected');
    
    // Уведомляем вторую сторону по WebSocket
    if (!callEndSent && typeof sendWebSocketMessage === 'function' && currentChat) {
        const token = localStorage.getItem('xipher_token');
        sendWebSocketMessage({
            type: 'call_end',
            token: token,
            target_user_id: currentChat.id
        });
        callEndSent = true;
    }
    closeCallModal();
    endCall();
}

// Отменить звонок
function cancelCall() {
    endCall();
}

// Завершить звонок
function endCall() {
    // Останавливаем звук звонка
    stopCallRingtone();
    hideCallControlsBar();
    hideInlineCallPrompt();
    stopCallResponsePolling();
    resetCallRecoveryState();
    stopAudioWatchdog(); // FIX: чистим сторож
    remoteScreenShareNotified = false; // FIX: сбрасываем флаг уведомления
    remoteScreenShareActive = false;
    remoteScreenShareTrackId = null;
    pendingGroupSfuConfig = null;
    resetRemoteMediaState(); // Сбрасываем индикаторы медиа
    if (sfuState.active) {
        stopSfuSession('endCall');
    }
    
    // Не шлем повторно, если уже отправили
    const targetId = currentChat ? currentChat.id : null;
    if (!callEndSent && targetId && typeof sendWebSocketMessage === 'function') {
        const token = localStorage.getItem('xipher_token');
        sendWebSocketMessage({
            type: isGroupCall ? 'group_call_end' : 'call_end',
            token: token,
            ...(isGroupCall ? { group_id: targetId } : { target_user_id: targetId })
        });
        callEndSent = true;
    }
    
    stopLocalAudioProcessing('endCall');
    stopRemoteAudioProcessing('endCall');
    if (rawLocalStream && rawLocalStream !== localStream) {
        rawLocalStream.getTracks().forEach(track => track.stop());
    }
    rawLocalStream = null;
    
    if (localStream) {
        localStream.getTracks().forEach(track => track.stop());
        localStream = null;
    }
    
    if (screenStream) {
        screenStream.getTracks().forEach(track => track.stop());
        screenStream = null;
    }
    
    // Закрываем одиночное соединение
    if (peerConnection) {
        if (peerConnection.__recoveryTimer) {
            clearTimeout(peerConnection.__recoveryTimer);
            peerConnection.__recoveryTimer = null;
        }
        peerConnection.close();
        peerConnection = null;
    }
    
    // Закрываем все групповые соединения
    if (isGroupCall && peerConnections.size > 0) {
        peerConnections.forEach((pc, userId) => {
            if (pc.__recoveryTimer) {
                clearTimeout(pc.__recoveryTimer);
                pc.__recoveryTimer = null;
            }
            pc.close();
            // Останавливаем потоки
            const stream = remoteStreams.get(userId);
            if (stream) {
                stream.getTracks().forEach(track => track.stop());
            }
        });
        peerConnections.clear();
        remoteStreams.clear();
    }
    
    // Очищаем список участников
    callParticipants.clear();
    groupActiveSenders.clear();
    
    isCallActive = false;
    isCallMinimized = false;
    currentCallId = null;
    isCaller = false;
    isGroupCall = false;
    stopCallTimer();
    stopCallResponsePolling();
    closeCallModal();
    
    // Закрываем минимизированное окно, если открыто
    const minimizedCall = document.getElementById('minimizedCall');
    if (minimizedCall) {
        minimizedCall.style.display = 'none';
    }
    
    // Очищаем контейнер участников
    const participantsContainer = document.getElementById('groupCallParticipants');
    if (participantsContainer) {
        participantsContainer.innerHTML = '';
    }
    
    // Отправляем уведомление о завершении звонка
    const wasGroupCall = isGroupCall; // Сохраняем значение перед очисткой
    const targetId2 = currentChat ? currentChat.id : null;
    if (wasGroupCall && currentChat) {
        // Для групповых звонков отправляем уведомление в группу
        if (!callEndSent && typeof sendWebSocketMessage === 'function') {
            const token = localStorage.getItem('xipher_token');
            sendWebSocketMessage({
                type: 'group_call_end',
                token: token,
                group_id: currentChat.id
            });
            callEndSent = true;
        }
    } else if (!wasGroupCall && currentChat) {
        if (!callEndSent) {
            sendCallResponse('ended');
            // Дополнительно уведомляем через WebSocket, чтобы собеседник закрыл звонок
            if (typeof sendWebSocketMessage === 'function' && targetId2) {
                const token = localStorage.getItem('xipher_token');
                sendWebSocketMessage({
                    type: 'call_end',
                    token: token,
                    target_user_id: targetId2
                });
            }
            callEndSent = true;
        }
    }
}

// Инициализировать peer connection
async function initPeerConnection(createOffer = true) {
    const configuration = {
        iceServers: buildIceServers(),
        iceCandidatePoolSize: 10
    };
    
    // Закрываем предыдущее соединение, если есть
    if (peerConnection) {
        peerConnection.close();
    }
    
    peerConnection = new RTCPeerConnection(configuration);
    
    // Добавляем локальный поток
    if (localStream) {
        localStream.getTracks().forEach(track => {
            peerConnection.addTrack(track, localStream);
        });
    }

    const hasLocalVideo = !!(localStream && localStream.getVideoTracks().length > 0);
    if (!hasLocalVideo) {
        ensureVideoTransceiverForScreenShare(peerConnection);
    }
    if (hasLocalVideo) {
        const videoSender = findVideoSender(peerConnection);
        await applyCameraEncodingForCall(videoSender);
    }
    
    // Обработка удаленного потока
    peerConnection.ontrack = (event) => {
        console.log('Получен удаленный поток:', event);
        remoteStream = resolveRemoteStreamFromTrack(event);
        
        // FIX: Ждем размьют трека и крепим поток безопасно
        event.track.onunmute = () => attachRemoteStream(remoteStream);
        attachRemoteStream(remoteStream);
        if (event.track.kind === 'video' && isScreenTrack(event.track)) {
            remoteScreenShareActive = true;
            remoteScreenShareTrackId = event.track.id || 'remote-screen';
            event.track.onended = () => {
                if (remoteScreenShareTrackId === (event.track.id || 'remote-screen')) {
                    remoteScreenShareActive = false;
                    remoteScreenShareTrackId = null;
                    remoteScreenShareNotified = false;
                }
            };
            if (screenStream) {
                notifications?.warning && notifications.warning('Собеседник начал демонстрацию экрана — остановили вашу, чтобы не перегружать звонок.');
                toggleScreenShare();
            }
            notifyRemoteScreenShare();
        }
        ensureRemoteFullscreenButton();
        
        const remoteVideo = document.getElementById('remoteVideo');
        const hasVideoTrack = remoteStream && remoteStream.getVideoTracks().length > 0;
        if (remoteVideo && (event.track.kind === 'video' || hasVideoTrack)) {
            // Ждем, пока элемент будет готов к воспроизведению
            const playVideo = () => {
                if (remoteVideo.readyState >= 2) { // HAVE_CURRENT_DATA или выше
                    const playPromise = remoteVideo.play();
                    if (playPromise !== undefined) {
                        playPromise.catch(err => {
                            // Игнорируем AbortError - это нормально при обновлении потока
                            if (err.name !== 'AbortError') {
                                console.error('Ошибка воспроизведения удаленного видео:', err);
                            }
                        });
                    }
                } else {
                    // Если элемент еще не готов, ждем события loadeddata
                    remoteVideo.addEventListener('loadeddata', () => {
                        const playPromise = remoteVideo.play();
                        if (playPromise !== undefined) {
                            playPromise.catch(err => {
                                if (err.name !== 'AbortError') {
                                    console.error('Ошибка воспроизведения удаленного видео:', err);
                                }
                            });
                        }
                    }, { once: true });
                }
            };
            
            // Пытаемся воспроизвести после небольшой задержки, чтобы поток успел установиться
            setTimeout(() => {
                playVideo();
                forceRemotePlayback();
            }, 100);
        }
        
        // Обновляем статус на "активный звонок", если еще не обновлен
        const statusElement = document.getElementById('callStatus') || document.querySelector('.call-status');
        if (statusElement && isCallActive) {
            const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
            statusElement.textContent = callTypeText;
        }
        
        // Запускаем таймер, если еще не запущен
        if (!callStartTime && isCallActive) {
            startCallTimer();
        }
        startConnectionWatchdog(peerConnection, 'single');
    };
    
    // Обработка ICE кандидатов с буферизацией
    peerConnection.onicecandidate = (event) => {
        if (event.candidate) {
            console.log('[Call] Отправляем ICE кандидат:', event.candidate.candidate?.substring(0, 50));
            sendIceCandidate(event.candidate);
        } else {
            console.log('[Call] Все ICE кандидаты собраны');
        }
    };
    
    // Отслеживание ICE gathering
    peerConnection.onicegatheringstatechange = () => {
        console.log('[Call] ICE gathering state:', peerConnection.iceGatheringState);
    };
    
    // Мониторим ICE и восстанавливаем соединение при обрыве
    peerConnection.oniceconnectionstatechange = () => {
        const state = peerConnection.iceConnectionState;
        console.log('[Call] ICE connection state:', state);
        
        if (state === 'checking') {
            console.log('[Call] ⏳ ICE checking...');
        } else if (state === 'connected' || state === 'completed') {
            console.log('[Call] ✅ ICE connected');
            clearCallRecoveryState('ice-connected');
            attachRemoteStream(remoteStream);
            startConnectionWatchdog(peerConnection, 'single');
            processBufferedIceCandidates();
        } else if (state === 'failed') {
            console.warn('[Call] ❌ ICE failed');
            beginCallRecovery(peerConnection, 'ice-failed');
            restartIceSafe(peerConnection);
            setTimeout(forceRemotePlayback, 150);
        } else if (state === 'disconnected') {
            console.warn('[Call] ⚠️ ICE disconnected');
            setTimeout(() => {
                if (peerConnection && peerConnection.iceConnectionState === 'disconnected') {
                    restartIceSafe(peerConnection);
                }
            }, 3000);
        }
    };
    
    // Обработка состояний соединения с улучшенным восстановлением
    peerConnection.onconnectionstatechange = () => {
        const state = peerConnection.connectionState;
        console.log('[Call] Connection state:', state);
        
        if (state === 'connecting') {
            console.log('[Call] ⏳ Connecting...');
        } else if (state === 'connected') {
            console.log('[Call] ✅ Connection established');
            clearCallRecoveryState('connected');
            processBufferedIceCandidates();
            
            // Обновляем статус на "активный звонок"
            const statusElement = document.getElementById('callStatus') || document.querySelector('.call-status');
            if (statusElement) {
                const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
                statusElement.textContent = callTypeText;
            }
            
            // Убеждаемся, что удаленное видео воспроизводится
            const remoteVideo = document.getElementById('remoteVideo');
            if (remoteVideo && remoteStream) {
                remoteVideo.muted = true;
                applyRemoteOutputVolume();
                // Проверяем готовность элемента перед воспроизведением
                if (remoteVideo.readyState >= 2) {
                    forceRemotePlayback();
                } else {
                    // Ждем, пока элемент будет готов
                    remoteVideo.addEventListener('loadeddata', () => {
                        forceRemotePlayback();
                    }, { once: true });
                }
            }
            
            // FIX: запускаем сторож тишины на установке соединения
            startAudioWatchdog();
            startConnectionWatchdog(peerConnection, 'single');
            
            if (!callStartTime && isCallActive) {
                startCallTimer();
            }
        } else if (state === 'failed') {
            console.warn('[Call] ❌ Connection failed');
            beginCallRecovery(peerConnection, 'conn-failed');
            restartIceSafe(peerConnection);
            setTimeout(forceRemotePlayback, 150);
        } else if (state === 'disconnected') {
            console.warn('[Call] ⚠️ Connection disconnected');
            setTimeout(() => {
                if (peerConnection && peerConnection.connectionState === 'disconnected') {
                    beginCallRecovery(peerConnection, 'conn-disconnected');
                    restartIceSafe(peerConnection);
                }
            }, 2000);
        }
    };
    
    // Создаем offer только если мы инициатор звонка
    if (createOffer) {
        try {
            const wantsVideo = currentCallType === 'video' || !!findVideoTransceiver(peerConnection);
            const offer = await peerConnection.createOffer({
                offerToReceiveAudio: true,
                offerToReceiveVideo: wantsVideo
            });
            await peerConnection.setLocalDescription(offer);
            console.log('Создан offer:', offer);
            
            // Отправляем offer через WebSocket
            await sendOffer(offer);
        } catch (error) {
            console.error('Ошибка создания offer:', error);
            notifications.error('Не удалось создать предложение соединения');
            endCall();
        }
    }
}

// Инициализировать peer connection для группового звонка
async function initGroupPeerConnection(userId, createOffer = true) {
    const configuration = {
        iceServers: buildIceServers(),
        iceCandidatePoolSize: 10
    };
    
    // Закрываем предыдущее соединение с этим пользователем, если есть
    if (peerConnections.has(userId)) {
        peerConnections.get(userId).close();
    }
    
    const pc = new RTCPeerConnection(configuration);
    peerConnections.set(userId, pc);
    
    // Добавляем локальный поток (аудио) только если не превышаем лимит активных отправителей
    const canSend = groupActiveSenders.size < MAX_GROUP_ACTIVE_SENDERS;
    if (localStream && canSend) {
        localStream.getAudioTracks().forEach(track => {
            pc.addTrack(track, localStream);
        });
        groupActiveSenders.add(localStorage.getItem('xipher_user_id') || 'me');
    } else {
        console.warn('[GroupCall] Joining as listener (no upstream audio) due to limit or missing stream');
    }
    
    // Обработка удаленного потока
    pc.ontrack = (event) => {
        console.log('[GroupCall] Получен поток от пользователя:', userId, event);
        const stream = event.streams[0];
        remoteStreams.set(userId, stream);
        
        // Добавляем видео элемент для этого участника
        addParticipantVideo(userId, stream);
        
        // Добавляем в список участников
        callParticipants.add(userId.toString());
        updateParticipantsCount();
        startConnectionWatchdog(pc, `group-${userId}`);
    };
    
    // Обработка ICE кандидатов
    pc.onicecandidate = (event) => {
        if (event.candidate) {
            console.log('[GroupCall] Отправляем ICE кандидат для пользователя:', userId);
            sendGroupIceCandidate(userId, event.candidate);
        }
    };
    
    // Обработка состояний соединения
    pc.onconnectionstatechange = () => {
        console.log('[GroupCall] Состояние соединения с', userId, ':', pc.connectionState);
        if (pc.connectionState === 'failed' || pc.connectionState === 'disconnected') {
            console.error('[GroupCall] Соединение потеряно с пользователем:', userId);
            restartIceSafe(pc);
            if (!pc.__recoveryTimer) {
                pc.__recoveryTimer = setTimeout(() => {
                    if (pc && (pc.connectionState === 'failed' || pc.connectionState === 'disconnected')) {
                        removeParticipant(userId);
                    }
                    pc.__recoveryTimer = null;
                }, GROUP_RECOVERY_GRACE_MS);
            }
        } else if (pc.connectionState === 'connected') {
            console.log('[GroupCall] Соединение установлено с пользователем:', userId);
            if (pc.__recoveryTimer) {
                clearTimeout(pc.__recoveryTimer);
                pc.__recoveryTimer = null;
            }
            callParticipants.add(userId.toString());
            updateParticipantsCount();
            startConnectionWatchdog(pc, `group-${userId}`);
        }
    };
    
    pc.oniceconnectionstatechange = () => {
        console.log('[GroupCall] Состояние ICE с', userId, ':', pc.iceConnectionState);
        if (pc.iceConnectionState === 'failed') {
            console.error('[GroupCall] ICE соединение не удалось с пользователем:', userId);
            restartIceSafe(pc);
            if (!pc.__recoveryTimer) {
                pc.__recoveryTimer = setTimeout(() => {
                    if (pc && pc.iceConnectionState === 'failed') {
                        removeParticipant(userId);
                    }
                    pc.__recoveryTimer = null;
                }, GROUP_RECOVERY_GRACE_MS);
            }
        } else if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
            if (pc.__recoveryTimer) {
                clearTimeout(pc.__recoveryTimer);
                pc.__recoveryTimer = null;
            }
            startConnectionWatchdog(pc, `group-${userId}`);
        }
    };
    
    // Создаем offer только если мы инициатор звонка
    if (createOffer) {
        try {
            const offer = await pc.createOffer({
                offerToReceiveAudio: true,
                offerToReceiveVideo: currentCallType === 'video'
            });
            await pc.setLocalDescription(offer);
            console.log('[GroupCall] Создан offer для пользователя:', userId);
            
            // Отправляем offer через WebSocket
            await sendGroupOffer(userId, offer);
        } catch (error) {
            console.error('[GroupCall] Ошибка создания offer для пользователя', userId, ':', error);
        }
    }
}

// Воспроизведение звука звонка
function playCallRingtone() {
    // Останавливаем предыдущий звук, если есть
    stopCallRingtone();
    
    console.log('[Call] Playing ringtone');
    
    // Создаем реалистичный звук звонка через Web Audio API
    try {
        // Создаем AudioContext (может потребоваться взаимодействие пользователя)
        let audioContext = null;
        try {
            audioContext = new (window.AudioContext || window.webkitAudioContext)();
        } catch (e) {
            console.warn('[Call] AudioContext creation failed, trying to resume:', e);
            // Пытаемся возобновить контекст (может потребоваться взаимодействие пользователя)
            if (audioContext && audioContext.state === 'suspended') {
                audioContext.resume().catch(err => {
                    console.warn('[Call] Could not resume AudioContext:', err);
                });
            }
        }
        
        if (!audioContext) {
            console.warn('[Call] AudioContext not available, using vibration only');
            // Fallback: только вибрация
            if (navigator.vibrate) {
                callRingtoneInterval = setInterval(() => {
                    if (callRingtoneInterval) {
                        navigator.vibrate([200, 100, 200, 100, 200]);
                    }
                }, 2000);
            }
            return;
        }
        
        const playTone = () => {
            try {
                // Возобновляем контекст, если он приостановлен
                if (audioContext.state === 'suspended') {
                    audioContext.resume().catch(err => {
                        console.warn('[Call] Could not resume AudioContext:', err);
                    });
                }
            
            // Создаем два осциллятора для более реалистичного звука звонка
            const osc1 = audioContext.createOscillator();
            const osc2 = audioContext.createOscillator();
            const gainNode = audioContext.createGain();
            
            osc1.connect(gainNode);
            osc2.connect(gainNode);
            gainNode.connect(audioContext.destination);
            
                // Две частоты для более реалистичного звука звонка (как в телефоне)
            osc1.frequency.value = 800;
            osc2.frequency.value = 1000;
            osc1.type = 'sine';
            osc2.type = 'sine';
            
                // Плавное нарастание и затухание (более громкий звук)
            const now = audioContext.currentTime;
            gainNode.gain.setValueAtTime(0, now);
                gainNode.gain.linearRampToValueAtTime(0.5, now + 0.1); // Увеличиваем громкость
                gainNode.gain.setValueAtTime(0.5, now + 0.3);
            gainNode.gain.linearRampToValueAtTime(0, now + 0.4);
            
            osc1.start(now);
            osc2.start(now);
            osc1.stop(now + 0.4);
            osc2.stop(now + 0.4);
            } catch (e) {
                console.error('[Call] Error playing tone:', e);
            }
        };
        
        // Играем сразу
        playTone();
        
        // Повторяем звук каждые 2 секунды
        callRingtoneInterval = setInterval(() => {
            if (callRingtoneInterval) {
                playTone();
            }
        }, 2000);
        
        // Вибрация для мобильных устройств
        if (navigator.vibrate) {
            navigator.vibrate([200, 100, 200, 100, 200]);
        }
    } catch (e) {
        console.error('[Call] Error creating ringtone:', e);
        // Fallback: вибрация
        if (navigator.vibrate) {
            callRingtoneInterval = setInterval(() => {
                if (callRingtoneInterval) {
                    navigator.vibrate([200, 100, 200]);
                }
            }, 2000);
        }
    }
}

// Остановка звука звонка
function stopCallRingtone() {
    if (callRingtoneInterval) {
        clearInterval(callRingtoneInterval);
        callRingtoneInterval = null;
    }
    callRingtone = null;
}

// Показать красивое уведомление о входящем звонке
function showCallNotification(message, fromUserId, isGroupCall = false, groupData = null) {
    console.log('[CallNotification] showCallNotification called:', { message, fromUserId, isGroupCall });
    
    // Удаляем предыдущее уведомление, если есть
    const existingNotification = document.querySelector('.call-notification');
    if (existingNotification) {
        console.log('[CallNotification] Removing existing notification');
        existingNotification.remove();
    }
    
    // Создаем красивое уведомление
    const notification = document.createElement('div');
    notification.className = 'call-notification';
    notification.style.position = 'fixed';
    notification.style.top = '20px';
    notification.style.right = '20px';
    notification.style.background = 'rgba(0, 0, 0, 0.95)';
    notification.style.border = '2px solid #bb86fc';
    notification.style.borderRadius = '12px';
    notification.style.padding = '20px';
    notification.style.zIndex = '99999';
    notification.style.minWidth = '300px';
    notification.style.maxWidth = '400px';
    notification.style.boxShadow = '0 4px 20px rgba(187, 134, 252, 0.3)';
    notification.style.animation = 'slideInRight 0.3s ease-out';
    notification.style.display = 'block'; // Явно устанавливаем display
    notification.style.visibility = 'visible'; // Явно устанавливаем visibility
    
    const title = document.createElement('div');
    title.style.color = '#bb86fc';
    title.style.fontSize = '18px';
    title.style.fontWeight = 'bold';
    title.style.marginBottom = '10px';
    title.style.display = 'flex';
    title.style.alignItems = 'center';
    title.style.gap = '8px';
    title.innerHTML = CALL_ICONS.phone + (isGroupCall ? ' Входящий групповой звонок' : ' Входящий звонок');
    
    const text = document.createElement('div');
    text.style.color = '#fff';
    text.style.marginBottom = '15px';
    text.style.fontSize = '14px';
    text.textContent = message;

    const buttons = document.createElement('div');
    buttons.style.display = 'flex';
    buttons.style.gap = '10px';
    buttons.style.justifyContent = 'flex-end';

    const acceptBtn = document.createElement('button');
    acceptBtn.textContent = 'Принять';
    acceptBtn.style.padding = '10px 20px';
    acceptBtn.style.background = '#4caf50';
    acceptBtn.style.color = '#fff';
    acceptBtn.style.border = 'none';
    acceptBtn.style.borderRadius = '8px';
    acceptBtn.style.cursor = 'pointer';
    acceptBtn.style.fontWeight = 'bold';
    acceptBtn.style.transition = 'all 0.2s';

    acceptBtn.onmouseover = () => {
        acceptBtn.style.background = '#45a049';
        acceptBtn.style.transform = 'scale(1.05)';
    };
    acceptBtn.onmouseout = () => {
        acceptBtn.style.background = '#4caf50';
        acceptBtn.style.transform = 'scale(1)';
    };

    const rejectBtn = document.createElement('button');
    rejectBtn.textContent = 'Отклонить';
    rejectBtn.style.padding = '10px 20px';
    rejectBtn.style.background = '#f44336';
    rejectBtn.style.color = '#fff';
    rejectBtn.style.border = 'none';
    rejectBtn.style.borderRadius = '8px';
    rejectBtn.style.cursor = 'pointer';
    rejectBtn.style.fontWeight = 'bold';
    rejectBtn.style.transition = 'all 0.2s';

    rejectBtn.onmouseover = () => {
        rejectBtn.style.background = '#da190b';
        rejectBtn.style.transform = 'scale(1.05)';
    };
    rejectBtn.onmouseout = () => {
        rejectBtn.style.background = '#f44336';
        rejectBtn.style.transform = 'scale(1)';
    };

    buttons.appendChild(acceptBtn);
    buttons.appendChild(rejectBtn);
    
    notification.appendChild(title);
    notification.appendChild(text);
    notification.appendChild(buttons);
    
    // Убеждаемся, что body существует
    if (!document.body) {
        console.error('[CallNotification] document.body is null! Waiting for DOM...');
        // Ждем загрузки DOM
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => {
                if (document.body) {
                    document.body.appendChild(notification);
                    console.log('[CallNotification] Notification appended after DOMContentLoaded');
                }
            });
            return notification;
        }
    }
    
    // Добавляем уведомление в DOM
    try {
        document.body.appendChild(notification);
        console.log('[CallNotification] Notification appended to body successfully');
        
        // Inline-панель не используется - новый fullscreen UI содержит все кнопки
        
        // Проверяем, что уведомление действительно добавлено
        setTimeout(() => {
            const checkNotification = document.querySelector('.call-notification');
            if (checkNotification && checkNotification === notification) {
                const computedStyle = window.getComputedStyle(checkNotification);
                console.log('[CallNotification] ✅ Notification verified in DOM');
                console.log('[CallNotification] - display:', computedStyle.display);
                console.log('[CallNotification] - visibility:', computedStyle.visibility);
                console.log('[CallNotification] - z-index:', computedStyle.zIndex);
                console.log('[CallNotification] - position:', computedStyle.position);
                console.log('[CallNotification] - top:', computedStyle.top);
                console.log('[CallNotification] - right:', computedStyle.right);
                console.log('[CallNotification] - opacity:', computedStyle.opacity);
                console.log('[CallNotification] - width:', computedStyle.width);
                console.log('[CallNotification] - height:', computedStyle.height);
                
                // Принудительно устанавливаем стили, если они не применились
                if (computedStyle.display === 'none') {
                    console.error('[CallNotification] ⚠️ WARNING: display is none, forcing block');
                    notification.style.display = 'block';
                }
                if (computedStyle.visibility === 'hidden') {
                    console.error('[CallNotification] ⚠️ WARNING: visibility is hidden, forcing visible');
                    notification.style.visibility = 'visible';
                }
                if (parseInt(computedStyle.zIndex) < 99999) {
                    console.warn('[CallNotification] ⚠️ z-index is low:', computedStyle.zIndex);
                    notification.style.zIndex = '99999';
                }
            } else {
                console.error('[CallNotification] ❌ ERROR: Notification NOT found in DOM after append!');
                console.error('[CallNotification] - checkNotification:', checkNotification);
                console.error('[CallNotification] - notification:', notification);
                console.error('[CallNotification] - document.body:', document.body);
            }
        }, 100);
    } catch (error) {
        console.error('[CallNotification] ❌ ERROR appending notification:', error);
        console.error('[CallNotification] Error stack:', error.stack);
    }
    
    acceptBtn.addEventListener('click', async () => {
        notification.remove();
        stopCallRingtone();
        if (isGroupCall && groupData) {
            // Принимаем групповой звонок
            currentChat = groupData.group;
            isGroupCall = true;
            currentCallType = groupData.callType;
            await acceptCall();
        } else {
            // Принимаем обычный звонок
            await acceptCall();
        }
    });

    rejectBtn.addEventListener('click', () => {
        notification.remove();
        stopCallRingtone();
        if (isGroupCall) {
            if (typeof sendWebSocketMessage === 'function') {
                const token = localStorage.getItem('xipher_token');
                sendWebSocketMessage({
                    type: 'group_call_end',
                    token: token,
                    group_id: groupData ? groupData.group.id : null
                });
            }
        } else {
            rejectCall();
        }
    });

    // Автоматически удаляем уведомление через 30 секунд
    setTimeout(() => {
        if (notification.parentNode) {
            notification.style.animation = 'slideOutRight 0.3s ease-out';
            setTimeout(() => {
                notification.remove();
                stopCallRingtone();
            }, 300);
        }
    }, 30000);
    
    return notification;
}

async function handleCallReoffer(offer) {
    if (!peerConnection) {
        console.warn('[Call] Re-offer received, but peerConnection is missing');
        return;
    }
    try {
        const signalingState = peerConnection.signalingState;
        console.log('[Call] Re-offer received, signaling state:', signalingState);
        
        // Можем обработать re-offer только в состоянии stable или have-local-offer
        if (signalingState !== 'stable' && signalingState !== 'have-local-offer') {
            console.warn('[Call] Cannot process re-offer in state:', signalingState);
            return;
        }
        
        await peerConnection.setRemoteDescription(new RTCSessionDescription(offer));
        console.log('[Call] Re-offer remote description set, new state:', peerConnection.signalingState);
        
        // Проверяем что можем создать answer
        if (peerConnection.signalingState !== 'have-remote-offer') {
            console.warn('[Call] Unexpected state after setRemoteDescription:', peerConnection.signalingState);
            return;
        }
        
        const rawAnswer = await peerConnection.createAnswer();
        const answer = {
            type: rawAnswer.type,
            sdp: stripCodecs(rawAnswer.sdp || '')
        };
        await peerConnection.setLocalDescription(answer);
        await sendAnswer(answer);
        clearCallRecoveryState('reoffer');
        startConnectionWatchdog(peerConnection, 'single');
        console.log('[Call] Re-offer handled');
    } catch (err) {
        console.error('[Call] Re-offer handling failed:', err);
    }
}

// Обработка входящего call_offer
async function handleCallOffer(data) {
    console.log('[Call] ========== handleCallOffer START ==========');
    console.log('[Call] Received call_offer data:', JSON.stringify(data, null, 2));
    console.log('[Call] isCallActive:', isCallActive);
    console.log('[Call] data.from_user_id:', data.from_user_id);

    const decodedOffer = data.offer ? decodeSignalingPayload(data.offer, data.offer_encoding) : null;
    const normalizedOffer = normalizeCallOfferPayload(decodedOffer);
    const isSamePeer = isCallActive && currentChat
        && data.from_user_id
        && currentChat.id.toString() === data.from_user_id.toString();
    if (isCallActive && isSamePeer && normalizedOffer) {
        await handleCallReoffer(normalizedOffer);
        return;
    }
    
    callEndSent = false;
    pendingIceCandidates = [];

    // Дедуп дубликатов offer: если пришло много подряд от того же пользователя
    const nowTs = Date.now();
    if (lastIncomingOffer && lastIncomingOffer.from_user_id === data.from_user_id && (nowTs - lastIncomingOfferAt) < 1500) {
        console.warn('[Call] Duplicate call_offer suppressed (recent from same user)');
        return;
    }
    
    // Гарантируем наличие модалки и элементов до дальнейших действий
    let modal = document.getElementById('callModal');
    if (!modal) {
        createCallModal();
        modal = document.getElementById('callModal');
    }
    
    if (isCallActive) {
        // Отклоняем входящий звонок, если уже в звонке
        console.log('[Call] Already in call, rejecting incoming call');
        if (typeof sendWebSocketMessage === 'function') {
            const token = localStorage.getItem('xipher_token');
            sendWebSocketMessage({
                type: 'call_end',
                token: token,
                target_user_id: data.from_user_id
            });
        }
        return;
    }
    
    // Получаем имя пользователя и аватар
    let callerName = 'Пользователь';
    let callerAvatarUrl = data.avatar_url || data.from_avatar_url || '';
    
    if (data.from_username) {
        callerName = data.from_username;
        console.log('[Call] Using from_username:', callerName);
    }
    
    // Ищем в списке друзей
    if (typeof friends !== 'undefined' && friends.length > 0) {
        const friend = friends.find(f => {
            const fId = f.id ? f.id.toString() : '';
            const fUserId = f.user_id ? f.user_id.toString() : '';
            const dataId = data.from_user_id ? data.from_user_id.toString() : '';
            return fId === dataId || fUserId === dataId;
        });
        if (friend) {
            if (!callerName || callerName === 'Пользователь') {
                callerName = friend.display_name || friend.username || friend.name || callerName;
            }
            if (!callerAvatarUrl) {
                callerAvatarUrl = friend.avatar_url || friend.avatarUrl || '';
            }
            console.log('[Call] Found in friends:', callerName, 'avatar:', callerAvatarUrl);
        }
    }
    
    // Ищем в списке чатов
    if (typeof chats !== 'undefined' && chats.length > 0) {
        const chat = chats.find(c => {
            const cId = c.id ? c.id.toString() : '';
            const cUserId = c.user_id ? c.user_id.toString() : '';
            const dataId = data.from_user_id ? data.from_user_id.toString() : '';
            return cId === dataId || cUserId === dataId;
        });
        if (chat) {
            if (!callerName || callerName === 'Пользователь') {
                callerName = chat.display_name || chat.name || callerName;
            }
            if (!callerAvatarUrl) {
                callerAvatarUrl = chat.avatar_url || chat.avatarUrl || '';
            }
            console.log('[Call] Found in chats:', callerName, 'avatar:', callerAvatarUrl);
        }
    }
    
    // Обновляем currentChat для корректной работы
    currentChat = {
        id: data.from_user_id.toString(),
        name: callerName,
        avatar_url: callerAvatarUrl
    };
    
    currentCallType = data.call_type || 'video';
    isCaller = false;
    
    console.log('[Call] Caller name determined:', callerName);
    console.log('[Call] Call type:', currentCallType);
    console.log('[Call] About to show notification...');
    
    // Парсим offer из строки, если нужно, с поддержкой b64 и битых escape
    if (data.offer) {
        if (!normalizedOffer) {
            console.error('[Call] Error parsing offer: unexpected payload');
            notifications?.error && notifications.error('Повреждённый offer, попросите переотправить звонок');
            return;
        }
        data.offer = normalizedOffer;
        if (typeof decodedOffer === 'string') {
            console.log('[Call] Parsed offer from string (sanitized)');
        } else if (decodedOffer && looksLikeSdp(decodedOffer)) {
            console.log('[Call] Parsed offer from raw SDP');
        }
    }
    window.pendingCallOffer = data;
    lastIncomingOffer = data;
    lastIncomingOfferAt = nowTs;
    console.log('[Call] pendingCallOffer saved');

    
    // Обеспечиваем появление встроенного окна звонка с кнопками Принять/Отклонить
    try {
        let modal = document.getElementById('callModal');
        if (!modal) {
            createCallModal();
            modal = document.getElementById('callModal');
        }
        showCallModal('incoming', currentCallType);
        if (modal) {
            modal.style.display = 'flex';
            modal.style.zIndex = '99999';
        }
    } catch (error) {
        console.error('[Call] Error ensuring call modal for incoming call:', error);
    }
    
    // Fullscreen UI уже содержит все кнопки принятия/отклонения

    // Запускаем звук звонка
    console.log('[Call] Calling playCallRingtone...');
    try {
        playCallRingtone();
        console.log('[Call] playCallRingtone called successfully');
    } catch (error) {
        console.error('[Call] Error calling playCallRingtone:', error);
    }
    
    console.log('[Call] ========== handleCallOffer END ==========');
}

// Обработка call_answer
async function handleCallAnswer(data) {
    if (!peerConnection) {
        console.warn('[Call] Answer получен, но peerConnection отсутствует');
        return;
    }
    
    try {
        // Парсим answer из строки/b64, если нужно
        let answer = decodeSignalingPayload(data.answer, data.answer_encoding);
        if (typeof answer === 'string') {
            const cleaned = sanitizeJsonString(answer);
            const parsed = safeJsonParse(cleaned);
            if (parsed) {
                answer = parsed;
            } else if (looksLikeSdp(answer)) {
                answer = { type: 'answer', sdp: answer };
            }
        }
        if (!answer || !answer.type || !answer.sdp) {
            console.error('[Call] Invalid answer payload:', answer);
            return;
        }
        await peerConnection.setRemoteDescription(new RTCSessionDescription(answer));
        console.log('Answer установлен успешно');
        
        // Применяем отложенные ICE после установки remoteDescription
        if (pendingIceCandidates.length > 0) {
            for (const cand of pendingIceCandidates) {
                try {
                    await peerConnection.addIceCandidate(new RTCIceCandidate(cand));
                } catch (e) {
                    console.error('Ошибка добавления отложенного ICE:', e);
                }
            }
            pendingIceCandidates = [];
        }
        
        // Переводим UI инициатора в активный звонок
        isCallActive = true;
        showCallModal('active', currentCallType);
        startCallTimer();
        forceRemotePlayback();
        
        // Отправляем начальное состояние медиа собеседнику
        setTimeout(() => sendInitialMediaState(), 500);
    } catch (error) {
        console.error('Ошибка обработки answer:', error);
    }
}

// Обработка call_ice_candidate с улучшенной надёжностью
async function handleCallIceCandidate(data) {
    if (!peerConnection) {
        console.warn('[Call] ICE получен, но peerConnection отсутствует - буферизуем');
        bufferIceCandidateForLater(data);
        return;
    }
    
    try {
        // Парсим candidate из строки/b64, если нужно
        let candidate = decodeSignalingPayload(data.candidate, data.candidate_encoding);
        if (typeof candidate === 'string') {
            const cleaned = sanitizeJsonString(candidate);
            const parsed = safeJsonParse(cleaned);
            if (parsed) {
                candidate = parsed;
            } else if (candidate.includes('candidate:')) {
                candidate = { candidate, sdpMid: '0', sdpMLineIndex: 0 };
            }
        }
        if (!candidate || !candidate.candidate) {
            console.warn('[Call] Invalid ICE payload:', candidate);
            return;
        }
        
        // Не добавляем ICE, пока не установлена удаленная сторона
        if (!peerConnection.remoteDescription || !peerConnection.remoteDescription.type) {
            console.log('[Call] Буферизуем ICE, remoteDescription ещё не установлена');
            pendingIceCandidates.push(candidate);
            pendingIceCandidatesWithTs.push({ candidate, ts: Date.now() });
            // Запускаем отложенную обработку
            scheduleBufferedIceProcessing();
            return;
        }
        
        await addIceCandidateWithRetry(peerConnection, candidate);
        console.log('[Call] ICE кандидат добавлен успешно');
    } catch (error) {
        console.error('[Call] Ошибка обработки ICE кандидата:', error);
        // При ошибке всё равно буферизуем
        if (data.candidate) {
            pendingIceCandidates.push(data);
        }
    }
}

// Буферизация ICE для случая когда peerConnection ещё не создан
function bufferIceCandidateForLater(data) {
    pendingIceCandidatesWithTs.push({ data, ts: Date.now() });
    // Очистка старых кандидатов
    const now = Date.now();
    pendingIceCandidatesWithTs = pendingIceCandidatesWithTs.filter(
        item => (now - item.ts) < ICE_BUFFER_TIMEOUT_MS
    );
}

// Отложенная обработка буферизованных ICE
let bufferedIceProcessingTimer = null;
function scheduleBufferedIceProcessing() {
    if (bufferedIceProcessingTimer) return;
    bufferedIceProcessingTimer = setTimeout(async () => {
        bufferedIceProcessingTimer = null;
        await processBufferedIceCandidates();
    }, 500);
}

async function processBufferedIceCandidates() {
    if (!peerConnection || !peerConnection.remoteDescription || !peerConnection.remoteDescription.type) {
        // Ещё не готовы, перепланируем
        if (pendingIceCandidates.length > 0) {
            scheduleBufferedIceProcessing();
        }
        return;
    }
    
    const candidates = [...pendingIceCandidates];
    pendingIceCandidates = [];
    
    for (const cand of candidates) {
        try {
            await addIceCandidateWithRetry(peerConnection, cand);
            console.log('[Call] Отложенный ICE добавлен');
        } catch (e) {
            console.warn('[Call] Не удалось добавить отложенный ICE:', e);
        }
    }
}

// Добавление ICE с retry
async function addIceCandidateWithRetry(pc, candidate, retries = 3) {
    for (let i = 0; i < retries; i++) {
        try {
            await pc.addIceCandidate(new RTCIceCandidate(candidate));
            return;
        } catch (err) {
            if (i === retries - 1) throw err;
            await new Promise(r => setTimeout(r, 100 * (i + 1)));
        }
    }
}

// Обработка call_end
function handleCallEnd() {
    // Останавливаем звук звонка
    stopCallRingtone();
    callEndSent = true;
    
    if (isCallActive) {
        notifications.info('Звонок завершен');
    }
    endCall();
}

// Отправить offer
// Отправить offer с retry для надёжности
async function sendOffer(offer) {
    if (typeof sendWebSocketMessage !== 'function') {
        console.error('[Call] sendWebSocketMessage not available');
        return;
    }
    
    const token = localStorage.getItem('xipher_token');
    
    // Убеждаемся, что target_user_id - это строка (UUID)
    let targetUserId = currentChat?.id;
    if (!targetUserId) {
        console.error('[Call] No target user for offer');
        return;
    }
    if (typeof targetUserId === 'number') {
        targetUserId = targetUserId.toString();
    }
    const offerJson = JSON.stringify(offer);
    const offerB64 = btoa(unescape(encodeURIComponent(offerJson)));

    const message = {
        type: 'call_offer',
        token: token,
        target_user_id: targetUserId,
        receiver_id: targetUserId,
        call_type: currentCallType || 'video',
        offer: offerB64,
        offer_encoding: 'b64',
        ts: Date.now()
    };
    
    console.log('[Call] Sending offer. target_user_id:', message.target_user_id);
    
    // Отправка с retry
    for (let attempt = 0; attempt < 3; attempt++) {
        try {
            sendWebSocketMessage(message);
            return;
        } catch (err) {
            console.warn(`[Call] Offer send attempt ${attempt + 1} failed:`, err);
            if (attempt < 2) {
                await new Promise(r => setTimeout(r, 150 * (attempt + 1)));
            }
        }
    }
}

// Отправить answer с retry
async function sendAnswer(answer) {
    if (typeof sendWebSocketMessage !== 'function') {
        console.error('[Call] sendWebSocketMessage not available');
        return;
    }
    
    const token = localStorage.getItem('xipher_token');
    const targetUserId = currentChat?.id?.toString();
    if (!targetUserId) {
        console.error('[Call] No target user for answer');
        return;
    }
    
    const answerJson = JSON.stringify(answer);
    const answerB64 = btoa(unescape(encodeURIComponent(answerJson)));
    
    const message = {
        type: 'call_answer',
        token: token,
        target_user_id: targetUserId,
        receiver_id: targetUserId,
        answer: answerB64,
        answer_encoding: 'b64',
        ts: Date.now()
    };
    
    // Отправка с retry
    for (let attempt = 0; attempt < 3; attempt++) {
        try {
            sendWebSocketMessage(message);
            return;
        } catch (err) {
            console.warn(`[Call] Answer send attempt ${attempt + 1} failed:`, err);
            if (attempt < 2) {
                await new Promise(r => setTimeout(r, 150 * (attempt + 1)));
            }
        }
    }
}

// Отправить ICE кандидата с гарантией доставки
async function sendIceCandidate(candidate) {
    if (typeof sendWebSocketMessage !== 'function') {
        console.error('[Call] sendWebSocketMessage not available');
        return;
    }
    
    const token = localStorage.getItem('xipher_token');
    
    // Убеждаемся, что target_user_id - это строка (UUID)
    let targetUserId = currentChat?.id;
    if (!targetUserId) {
        console.warn('[Call] No target user for ICE candidate');
        return;
    }
    if (typeof targetUserId === 'number') {
        targetUserId = targetUserId.toString();
    }

    const candJson = JSON.stringify(candidate);
    const candB64 = btoa(unescape(encodeURIComponent(candJson)));

    const message = {
        type: 'call_ice_candidate',
        token: token,
        target_user_id: targetUserId,
        receiver_id: targetUserId,
        candidate: candB64,
        candidate_encoding: 'b64',
        ts: Date.now() // Для дедупликации
    };

    // Отправка с retry при ошибке
    for (let attempt = 0; attempt < 3; attempt++) {
        try {
            sendWebSocketMessage(message);
            return;
        } catch (err) {
            console.warn(`[Call] ICE send attempt ${attempt + 1} failed:`, err);
            if (attempt < 2) {
                await new Promise(r => setTimeout(r, 100 * (attempt + 1)));
            }
        }
    }
}

// Отправить групповой offer
async function sendGroupOffer(userId, offer) {
    if (typeof sendWebSocketMessage === 'function') {
        const token = localStorage.getItem('xipher_token');
        sendWebSocketMessage({
            type: 'group_call_offer',
            token: token,
            group_id: currentChat.id,
            target_user_id: userId,
            call_type: currentCallType || 'video',
            offer: JSON.stringify(offer)
        });
    } else {
        console.error('sendWebSocketMessage not available');
    }
}

// Отправить групповой answer
async function sendGroupAnswer(userId, answer) {
    if (typeof sendWebSocketMessage === 'function') {
        const token = localStorage.getItem('xipher_token');
        sendWebSocketMessage({
            type: 'group_call_answer',
            token: token,
            group_id: currentChat.id,
            target_user_id: userId,
            answer: JSON.stringify(answer)
        });
    } else {
        console.error('sendWebSocketMessage not available');
    }
}

// Отправить групповой ICE кандидат
async function sendGroupIceCandidate(userId, candidate) {
    if (typeof sendWebSocketMessage === 'function') {
        const token = localStorage.getItem('xipher_token');
        sendWebSocketMessage({
            type: 'group_call_ice_candidate',
            token: token,
            group_id: currentChat.id,
            target_user_id: userId,
            candidate: JSON.stringify(candidate)
        });
    } else {
        console.error('sendWebSocketMessage not available');
    }
}

// Отправить состояние медиа (микрофон/камера) собеседнику
function sendMediaState(mediaType, enabled) {
    if (!isCallActive || !currentChat) return;
    
    if (typeof sendWebSocketMessage === 'function') {
        const token = localStorage.getItem('xipher_token');
        const message = {
            type: 'call_media_state',
            token: token,
            target_user_id: currentChat.id,
            media_type: mediaType,
            enabled: enabled
        };
        
        if (isGroupCall) {
            message.type = 'group_call_media_state';
            message.group_id = currentChat.id;
            delete message.target_user_id;
        }
        
        sendWebSocketMessage(message);
        console.log('[Call] Sent media state:', mediaType, enabled);
    }
}

// Отправить начальное состояние всех медиа собеседнику
function sendInitialMediaState() {
    if (!isCallActive || !currentChat) {
        console.log('[Call] sendInitialMediaState: call not active yet, delaying...');
        return;
    }
    
    // Получаем текущее состояние микрофона
    const micEnabled = localStream && localStream.getAudioTracks().some(t => t.enabled);
    sendMediaState('mic', micEnabled !== false);
    
    // Получаем текущее состояние камеры
    const videoEnabled = localStream && localStream.getVideoTracks().some(t => t.enabled);
    sendMediaState('video', videoEnabled === true);
    
    // Получаем состояние динамиков
    const remoteVideo = document.getElementById('remoteVideo');
    const remoteAudio = document.getElementById('remoteAudio');
    const speakerEnabled = (remoteVideo && !remoteVideo.muted) || (remoteAudio && !remoteAudio.muted);
    sendMediaState('speaker', speakerEnabled !== false);
    
    console.log('[Call] Sent initial media state - mic:', micEnabled, 'video:', videoEnabled, 'speaker:', speakerEnabled);
}

// Обновить UI индикаторы состояния медиа удаленного пользователя
function updateRemoteMediaIndicators() {
    const container = document.getElementById('singleCallContainer') || document.getElementById('callVideoPlaceholder')?.parentElement;
    if (!container) return;
    
    // Создаём или находим панель индикаторов
    let indicators = document.getElementById('remoteMediaIndicators');
    if (!indicators) {
        indicators = document.createElement('div');
        indicators.id = 'remoteMediaIndicators';
        indicators.className = 'remote-media-indicators';
        container.appendChild(indicators);
    }
    
    // Обновляем индикаторы
    indicators.innerHTML = '';
    
    // Микрофон
    if (!remoteMediaState.micEnabled) {
        const micIndicator = document.createElement('div');
        micIndicator.className = 'remote-media-indicator mic-off';
        micIndicator.innerHTML = CALL_ICONS.micOff;
        micIndicator.title = 'Микрофон выключен';
        indicators.appendChild(micIndicator);
    }
    
    // Камера
    if (!remoteMediaState.videoEnabled) {
        const videoIndicator = document.createElement('div');
        videoIndicator.className = 'remote-media-indicator video-off';
        videoIndicator.innerHTML = CALL_ICONS.videoOff;
        videoIndicator.title = 'Камера выключена';
        indicators.appendChild(videoIndicator);
    }
    
    // Показываем/скрываем панель
    indicators.style.display = (indicators.children.length > 0) ? 'flex' : 'none';
    
    // Также обновляем placeholder если камера выключена
    const placeholder = document.getElementById('callVideoPlaceholder');
    const remoteVideo = document.getElementById('remoteVideo');
    if (placeholder && remoteVideo) {
        // Показываем placeholder если у собеседника выключена камера
        if (!remoteMediaState.videoEnabled && currentCallType === 'video') {
            placeholder.style.display = 'flex';
            remoteVideo.style.opacity = '0.3';
        } else {
            // Проверяем есть ли видео трек
            const hasVideoTrack = remoteStream && remoteStream.getVideoTracks().length > 0;
            placeholder.style.display = hasVideoTrack ? 'none' : 'flex';
            remoteVideo.style.opacity = hasVideoTrack ? '1' : '0.3';
        }
    }
}

// Обработать входящее сообщение о состоянии медиа
function handleRemoteMediaState(data) {
    console.log('[Call] Received remote media state:', data);
    
    const mediaType = data.media_type;
    const enabled = data.enabled;
    
    if (mediaType === 'mic') {
        remoteMediaState.micEnabled = enabled;
    } else if (mediaType === 'video') {
        remoteMediaState.videoEnabled = enabled;
    } else if (mediaType === 'speaker') {
        remoteMediaState.speakerEnabled = enabled;
    }
    
    updateRemoteMediaIndicators();
}

// Сбросить состояние медиа при новом звонке
function resetRemoteMediaState() {
    remoteMediaState = {
        micEnabled: true,
        videoEnabled: true,
        speakerEnabled: true
    };
    updateRemoteMediaIndicators();
}

// Отправить ответ на звонок
async function sendCallResponse(response) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        await fetch(API_BASE + '/api/call-response', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token: token,
                receiver_id: currentChat.id,
                response: response
            })
        });
    } catch (error) {
        console.error('Ошибка отправки ответа на звонок:', error);
    }
}

// Проверка сигналинга больше не нужна - используем WebSocket
// Оставлено для совместимости, но не используется
function startSignalingPolling() {
    // WebSocket обрабатывает сигналинг автоматически
}

function stopSignalingPolling() {
    // WebSocket обрабатывает сигналинг автоматически
}

// Переключить микрофон
function toggleMicrophone() {
    if (localStream) {
        const audioTracks = localStream.getAudioTracks();
        if (!audioTracks.length) return;
        const nextEnabled = !audioTracks[0].enabled;
        audioTracks.forEach(track => {
            track.enabled = nextEnabled;
        });
        if (rawLocalStream) {
            rawLocalStream.getAudioTracks().forEach(track => {
                track.enabled = nextEnabled;
            });
        }
        
        const buttons = document.querySelectorAll('#toggleMicBtn, #toggleMicBtnGroup');
        buttons.forEach(btn => {
            btn.style.opacity = nextEnabled ? '1' : '0.5';
            btn.innerHTML = nextEnabled ? callIcon('mic') : callIcon('micOff');
        });
        
        // Отправляем состояние собеседнику
        sendMediaState('mic', nextEnabled);
    }
}

function updateCallControlsOffset() {
    const root = document.documentElement;
    if (!root) return;
    if (!callControlsBar || callControlsBar.style.display === 'none') {
        root.style.setProperty('--call-controls-offset', '0px');
        return;
    }
    const rect = callControlsBar.getBoundingClientRect();
    const offset = Math.ceil(rect.height + 12);
    root.style.setProperty('--call-controls-offset', `${offset}px`);
}

function scheduleCallControlsOffsetUpdate() {
    if (typeof requestAnimationFrame === 'function') {
        requestAnimationFrame(updateCallControlsOffset);
    } else {
        updateCallControlsOffset();
    }
}

// Создать или получить плавающую панель управления
function ensureCallControlsBar() {
    if (callControlsBar) return callControlsBar;
    
    const bar = document.createElement('div');
    bar.id = 'callControlsBar';
    bar.style.position = 'fixed';
    bar.style.bottom = '20px';
    bar.style.right = '20px';
    bar.style.display = 'none';
    bar.style.gap = '10px';
    bar.style.padding = '10px 12px';
    bar.style.borderRadius = '12px';
    bar.style.background = 'rgba(0,0,0,0.65)';
    bar.style.backdropFilter = 'blur(6px)';
    bar.style.boxShadow = '0 10px 30px rgba(0,0,0,0.35)';
    bar.style.zIndex = '100000';
    bar.style.alignItems = 'center';
    bar.style.justifyContent = 'center';
    bar.style.flexWrap = 'wrap';
    bar.style.maxWidth = '360px';
    bar.style.fontSize = '20px';
    bar.style.color = '#fff';
    
    bar.innerHTML = `
        <button id="ctrlToggleMic" title="Микрофон" style="min-width:48px;padding:10px;border-radius:10px;border:1px solid #444;background:#222;color:#fff;cursor:pointer;display:flex;align-items:center;justify-content:center;">${CALL_ICONS.mic}</button>
        <button id="ctrlToggleVideo" title="Камера" style="min-width:48px;padding:10px;border-radius:10px;border:1px solid #444;background:#222;color:#fff;cursor:pointer;display:flex;align-items:center;justify-content:center;">${CALL_ICONS.video}</button>
        <button id="ctrlToggleScreen" title="Демонстрация экрана" style="min-width:48px;padding:10px;border-radius:10px;border:1px solid #444;background:#222;color:#fff;cursor:pointer;display:flex;align-items:center;justify-content:center;">${CALL_ICONS.screen}</button>
        <button id="ctrlToggleAudio" title="Звук собеседника" style="min-width:48px;padding:10px;border-radius:10px;border:1px solid #444;background:#222;color:#fff;cursor:pointer;display:flex;align-items:center;justify-content:center;">${CALL_ICONS.speaker}</button>
        <button id="ctrlEndCall" title="Завершить" style="min-width:48px;padding:10px;border-radius:10px;border:1px solid #a00;background:#d22;color:#fff;cursor:pointer;display:flex;align-items:center;justify-content:center;">${CALL_ICONS.hangup}</button>
    `;
    
    // Навешиваем хендлеры
    bar.querySelector('#ctrlToggleMic')?.addEventListener('click', toggleMicrophone);
    bar.querySelector('#ctrlToggleVideo')?.addEventListener('click', toggleVideo);
    bar.querySelector('#ctrlToggleScreen')?.addEventListener('click', toggleScreenShare);
    bar.querySelector('#ctrlToggleAudio')?.addEventListener('click', toggleAudio);
    bar.querySelector('#ctrlEndCall')?.addEventListener('click', endCall);
    
    document.body.appendChild(bar);
    callControlsBar = bar;
    if (!callControlsOffsetListenerAttached && typeof window !== 'undefined') {
        window.addEventListener('resize', updateCallControlsOffset);
        callControlsOffsetListenerAttached = true;
    }
    return bar;
}

function showCallControlsBar() {
    const bar = ensureCallControlsBar();
    bar.style.display = 'flex';
    scheduleCallControlsOffsetUpdate();
}

function hideCallControlsBar() {
    if (callControlsBar) {
        callControlsBar.style.display = 'none';
    }
    updateCallControlsOffset();
}

// Встроенная панель принятия/отклонения/завершения (fallback HTML)
function ensureInlineCallPrompt() {
    if (callInlinePrompt) return callInlinePrompt;
    
    const prompt = document.createElement('div');
    prompt.id = 'callInlinePrompt';
    prompt.style.position = 'fixed';
    prompt.style.bottom = '20px';
    prompt.style.left = '20px';
    prompt.style.zIndex = '100001';
    prompt.style.display = 'none';
    prompt.style.background = 'rgba(0,0,0,0.78)';
    prompt.style.color = '#fff';
    prompt.style.padding = '12px 14px';
    prompt.style.borderRadius = '12px';
    prompt.style.boxShadow = '0 10px 30px rgba(0,0,0,0.35)';
    prompt.style.backdropFilter = 'blur(6px)';
    prompt.style.minWidth = '260px';
    prompt.style.maxWidth = '320px';
    prompt.innerHTML = `
        <div style="font-weight:700;margin-bottom:6px" id="callInlineTitle">Входящий звонок</div>
        <div style="font-size:13px;margin-bottom:10px;opacity:0.85" id="callInlineDesc">Кто-то звонит...</div>
        <div style="display:flex;gap:8px;flex-wrap:wrap;">
            <button id="callInlineAccept" style="flex:1;min-width:90px;padding:8px 10px;border-radius:10px;border:none;background:#4caf50;color:#fff;font-weight:700;cursor:pointer;">Принять</button>
            <button id="callInlineReject" style="flex:1;min-width:90px;padding:8px 10px;border-radius:10px;border:none;background:#e53935;color:#fff;font-weight:700;cursor:pointer;">Отклонить</button>
            <button id="callInlineEnd" style="flex:1;min-width:90px;padding:8px 10px;border-radius:10px;border:none;background:#9e9e9e;color:#fff;font-weight:700;cursor:pointer;display:none;">Завершить</button>
        </div>
    `;
    
    prompt.querySelector('#callInlineAccept')?.addEventListener('click', () => {
        prompt.style.display = 'none';
        acceptCall();
    });
    prompt.querySelector('#callInlineReject')?.addEventListener('click', () => {
        prompt.style.display = 'none';
        rejectCall();
    });
    prompt.querySelector('#callInlineEnd')?.addEventListener('click', () => {
        prompt.style.display = 'none';
        endCall();
    });
    
    document.body.appendChild(prompt);
    callInlinePrompt = prompt;
    return prompt;
}

function showInlineCallPrompt(mode = 'incoming', title = 'Звонок', desc = '') {
    const prompt = ensureInlineCallPrompt();
    const titleEl = prompt.querySelector('#callInlineTitle');
    const descEl = prompt.querySelector('#callInlineDesc');
    const acceptBtn = prompt.querySelector('#callInlineAccept');
    const rejectBtn = prompt.querySelector('#callInlineReject');
    const endBtn = prompt.querySelector('#callInlineEnd');
    
    if (titleEl) titleEl.textContent = title;
    if (descEl) descEl.textContent = desc || '';
    
    if (mode === 'incoming') {
        acceptBtn.style.display = 'inline-flex';
        rejectBtn.style.display = 'inline-flex';
        endBtn.style.display = 'none';
    } else if (mode === 'outgoing') {
        acceptBtn.style.display = 'none';
        rejectBtn.style.display = 'none';
        endBtn.style.display = 'inline-flex';
        if (endBtn) endBtn.textContent = 'Отменить';
    } else if (mode === 'active') {
        acceptBtn.style.display = 'none';
        rejectBtn.style.display = 'none';
        endBtn.style.display = 'inline-flex';
        if (endBtn) endBtn.textContent = 'Завершить';
    }
    
    prompt.style.display = 'block';
}

function hideInlineCallPrompt() {
    if (callInlinePrompt) {
        callInlinePrompt.style.display = 'none';
    }
}

// Переключить видео
function toggleVideo() {
    if (localStream) {
        const videoTracks = localStream.getVideoTracks();
        videoTracks.forEach(track => {
            track.enabled = !track.enabled;
        });
        
        const isEnabled = videoTracks[0]?.enabled;
        const buttons = document.querySelectorAll('#toggleVideoBtn, #toggleVideoBtnGroup');
        buttons.forEach(btn => {
            btn.style.opacity = isEnabled ? '1' : '0.5';
            btn.innerHTML = isEnabled ? callIcon('video') : callIcon('videoOff');
        });
        
        // Отправляем состояние собеседнику
        sendMediaState('video', isEnabled);
    }
}

function findVideoTransceiver(pc) {
    if (!pc || typeof pc.getTransceivers !== 'function') return null;
    return pc.getTransceivers().find(t =>
        t.receiver && t.receiver.track && t.receiver.track.kind === 'video'
    ) || null;
}

function findVideoSender(pc) {
    if (!pc) return null;
    const sender = pc.getSenders().find(s => s.track && s.track.kind === 'video');
    if (sender) return sender;
    const transceiver = findVideoTransceiver(pc);
    return transceiver ? transceiver.sender : null;
}

function ensureVideoTransceiverForScreenShare(pc) {
    if (!pc || typeof pc.addTransceiver !== 'function') return;
    const hasVideoSender = pc.getSenders().some(s => s.track && s.track.kind === 'video');
    if (hasVideoSender || findVideoTransceiver(pc)) return;
    try {
        pc.addTransceiver('video', { direction: 'sendrecv' });
    } catch (err) {
        console.warn('[Call] Failed to add video transceiver:', err);
    }
}

async function applyScreenShareEncoding(sender, quality) {
    const resolved = quality || getCallQualitySettingForCall('screen');
    await applyVideoEncodingForCall(sender, 'screen', resolved);
}

// Переключить демонстрацию экрана
async function toggleScreenShare() {
    if (isGroupCall && sfuState.active && sfuState.publisher) {
        return toggleScreenShareSfu();
    }
    try {
        if (screenStream) {
            const hadScreenAudio = screenStream.getAudioTracks().length > 0;
            // Останавливаем демонстрацию экрана
            screenStream.getTracks().forEach(track => track.stop());
            screenStream = null;
            
            // Возвращаем камеру
            const restoreVideoTrack = async (pc) => {
                if (!pc || !localStream) return;
                const videoTracks = localStream.getVideoTracks();
                if (!videoTracks.length) return;
                const sender = findVideoSender(pc);
                if (sender) {
                    await sender.replaceTrack(videoTracks[0]);
                    await applyCameraEncodingForCall(sender);
                }
            };

            if (peerConnection) {
                await restoreVideoTrack(peerConnection);
            }
            if (isGroupCall && peerConnections.size > 0) {
                for (const [, pc] of peerConnections) {
                    await restoreVideoTrack(pc);
                }
            }

            if (hadScreenAudio && localStream) {
                const restoreAudioTrack = async (pc) => {
                    if (!pc) return;
                    const audioTracks = localStream.getAudioTracks();
                    if (!audioTracks.length) return;
                    const sender = pc.getSenders().find(s =>
                        s.track && s.track.kind === 'audio'
                    );
                    if (sender) {
                        await sender.replaceTrack(audioTracks[0]);
                    }
                };

                if (peerConnection) {
                    await restoreAudioTrack(peerConnection);
                }
                if (isGroupCall && peerConnections.size > 0) {
                    for (const [, pc] of peerConnections) {
                        await restoreAudioTrack(pc);
                    }
                }
            }

            // Возвращаем локальный превью на камеру
            const localVideo = document.getElementById('localVideo');
            if (localVideo) {
                localVideo.srcObject = localStream;
            }
        } else {
            // Начинаем демонстрацию экрана
            if (remoteScreenShareActive) {
                notifications?.warning && notifications.warning('Собеседник уже делится экраном — остановите его демонстрацию, чтобы избежать зависаний.');
                return;
            }
            screenStream = await navigator.mediaDevices.getDisplayMedia(buildScreenShareConstraintsForCall());
            
            const screenQuality = getCallQualitySettingForCall('screen');
            const videoTrack = screenStream.getVideoTracks()[0];
            const audioTrack = screenStream.getAudioTracks()[0];
            if (audioTrack) {
                await enhanceAudioTrack(audioTrack, 'screenShare');
            }
            if (videoTrack && typeof videoTrack.contentHint === 'string') {
                videoTrack.contentHint = 'detail';
            }
            
            // Заменяем видео трек
            const applyVideoTrack = async (pc, ensureTransceiver) => {
                if (!pc || !videoTrack) return;
                if (ensureTransceiver) {
                    ensureVideoTransceiverForScreenShare(pc);
                }
                const videoSender = findVideoSender(pc);
                if (videoSender) {
                    await videoSender.replaceTrack(videoTrack);
                    await applyScreenShareEncoding(videoSender, screenQuality);
                } else {
                    console.warn('[Call] No video sender available for screen share');
                }
            };

            if (peerConnection) {
                await applyVideoTrack(peerConnection, true);
            }
            if (isGroupCall && peerConnections.size > 0) {
                for (const [, pc] of peerConnections) {
                    await applyVideoTrack(pc, false);
                }
            }
            
            // Добавляем аудио трек, если есть
            if (audioTrack && localStream) {
                const applyAudioTrack = async (pc) => {
                    if (!pc) return;
                    const sender = pc.getSenders().find(s =>
                        s.track && s.track.kind === 'audio'
                    );
                    if (sender) {
                        await sender.replaceTrack(audioTrack);
                    }
                };

                // Одиночный звонок
                if (peerConnection) {
                    await applyAudioTrack(peerConnection);
                }
                // Групповой звонок: обновляем всем
                if (isGroupCall && peerConnections.size > 0) {
                    for (const [, pc] of peerConnections) {
                        await applyAudioTrack(pc);
                    }
                }
            }
            
            // Локальный превью экрана
            const localVideo = document.getElementById('localVideo');
            if (localVideo) {
                localVideo.srcObject = screenStream;
            }
            
            // Останавливаем демонстрацию при нажатии пользователя
            if (videoTrack) {
                videoTrack.onended = () => {
                    toggleScreenShare();
                };
            }
        }
        
        const buttons = document.querySelectorAll('#toggleScreenBtn, #toggleScreenBtnGroup');
        buttons.forEach(btn => {
            btn.style.opacity = screenStream ? '1' : '0.5';
            btn.innerHTML = screenStream ? callIcon('screenOff') : callIcon('screen');
            btn.title = screenStream ? 'Остановить демонстрацию' : 'Демонстрация экрана';
        });
    } catch (error) {
        console.error('Ошибка демонстрации экрана:', error);
        if (error.name !== 'NotAllowedError') {
            notifications.error('Не удалось начать демонстрацию экрана');
        }
    }
}

// Переключить звук
function toggleAudio() {
    const remoteVideo = document.getElementById('remoteVideo');
    const remoteAudio = document.getElementById('remoteAudio') || ensureRemoteAudioElement();
    
    // Основной источник звука — remoteAudio, если есть; иначе remoteVideo
    let isMuted;
    if (remoteAudio) {
        remoteAudio.muted = !remoteAudio.muted;
        isMuted = remoteAudio.muted;
    } else if (remoteVideo) {
        remoteVideo.muted = !remoteVideo.muted;
        isMuted = remoteVideo.muted;
    } else {
        isMuted = false;
    }
    
    // Синхронизируем второй элемент, если есть
    if (remoteAudio && typeof isMuted === 'boolean') {
        remoteAudio.muted = isMuted;
    }
    if (remoteVideo && typeof isMuted === 'boolean') {
        remoteVideo.muted = remoteAudio ? true : isMuted;
    }
    applyRemoteOutputVolume();
    if (typeof isMuted === 'boolean') {
        document.querySelectorAll('[id^="participant-video-"]').forEach(video => {
            video.muted = isMuted;
        });
    }
    
    const buttons = document.querySelectorAll('#toggleAudioBtn, #toggleAudioBtnGroup');
    buttons.forEach(btn => {
        btn.style.opacity = isMuted ? '0.5' : '1';
        btn.innerHTML = isMuted ? callIcon('speakerOff') : callIcon('speaker');
    });
}

// Закрыть модальное окно звонка
function closeCallModal() {
    const modal = document.getElementById('callModal');
    if (modal) {
        modal.classList.remove('is-visible', 'state-incoming', 'state-outgoing', 'state-active');
        delete modal.dataset.callState;
        modal.style.display = 'none';
    }
    hideCallControlsBar();
}

// Запустить таймер звонка
function startCallTimer() {
    // Останавливаем предыдущий таймер, если есть
    if (callTimer) {
        clearInterval(callTimer);
        callTimer = null;
    }
    
    // Не запускаем таймер, если звонок завершен
    if (callEndSent) return;
    
    callStartTime = Date.now();
    
    // Обновляем сразу
    updateCallTimer();
    
    callTimer = setInterval(() => {
        updateCallTimer();
    }, 1000);
}

// Обновить таймер звонка
function updateCallTimer() {
    if (!callStartTime) return;
    
    const elapsed = Math.floor((Date.now() - callStartTime) / 1000);
    const minutes = Math.floor(elapsed / 60);
    const seconds = elapsed % 60;
    const timeString = `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
    
    // Обновляем таймер для одиночного звонка
    const timerElement = document.getElementById('callTimer');
    if (timerElement) {
        timerElement.textContent = timeString;
    }
    
    // Обновляем таймер для группового звонка
    const groupTimerElement = document.getElementById('groupCallTimer');
    if (groupTimerElement) {
        groupTimerElement.textContent = timeString;
    }
    
    // Обновляем минимизированный таймер, если окно минимизировано
    if (isCallMinimized) {
        const minimizedTimer = document.getElementById('minimizedCallTimer');
        if (minimizedTimer) {
            minimizedTimer.textContent = timeString;
        }
    }
}

// Остановить таймер звонка
function stopCallTimer() {
    if (callTimer) {
        clearInterval(callTimer);
        callTimer = null;
    }
    callStartTime = null;
    
    // Сбрасываем таймер для одиночного звонка
    const timerElement = document.getElementById('callTimer');
    if (timerElement) {
        timerElement.textContent = '00:00';
    }
    
    // Сбрасываем таймер для группового звонка
    const groupTimerElement = document.getElementById('groupCallTimer');
    if (groupTimerElement) {
        groupTimerElement.textContent = '00:00';
    }
}

// Обработка входящего звонка
function handleIncomingCall(callData) {
    // Проверяем, не активен ли уже звонок
    if (isCallActive) {
        return; // Игнорируем, если уже идет звонок
    }
    
    currentCallType = callData.call_type || 'video';
    currentCallId = callData.id || null;
    isCaller = false;

    const callerName = callData.caller_username || 'Пользователь';
    const callerAvatarUrl = callData.caller_avatar_url || callData.avatar_url || '';
    
    if (callData.caller_id) {
        currentChat = {
            id: callData.caller_id,
            name: callerName,
            avatar_url: callerAvatarUrl
        };
    }
    
    showCallModal('incoming', currentCallType);
    
    // Обновляем имя и аватар вызывающего
    const callUserName = document.getElementById('callUserName');
    const callAvatar = document.getElementById('callAvatar');
    if (callUserName) {
        callUserName.textContent = callerName;
    }
    if (callAvatar) {
        const avatarImg = callAvatar.querySelector('img');
        const avatarFallback = callAvatar.querySelector('.avatar-fallback');
        if (callerAvatarUrl && avatarImg) {
            avatarImg.src = callerAvatarUrl;
            avatarImg.style.display = 'block';
            if (avatarFallback) avatarFallback.style.display = 'none';
        } else if (avatarFallback) {
            avatarFallback.textContent = callerName.charAt(0).toUpperCase();
            avatarFallback.style.display = 'flex';
            if (avatarImg) avatarImg.style.display = 'none';
        }
    }
    
    // Обновляем placeholder
    const placeholderAvatarImg = document.getElementById('placeholderAvatarImg');
    const placeholderInitial = document.getElementById('placeholderInitial');
    const placeholderName = document.getElementById('placeholderName');
    
    if (callerAvatarUrl && placeholderAvatarImg) {
        placeholderAvatarImg.src = callerAvatarUrl;
        placeholderAvatarImg.style.display = 'block';
        if (placeholderInitial) placeholderInitial.style.display = 'none';
    } else if (placeholderInitial) {
        placeholderInitial.textContent = callerName.charAt(0).toUpperCase();
        placeholderInitial.style.display = 'flex';
        if (placeholderAvatarImg) placeholderAvatarImg.style.display = 'none';
    }
    if (placeholderName) {
        placeholderName.textContent = callerName;
    }

    const minimizedName = document.querySelector('#minimizedCall .minimized-call-name');
    const minimizedAvatar = document.querySelector('#minimizedCall .minimized-call-avatar');
    if (minimizedName) {
        minimizedName.textContent = callerName;
    }
    if (minimizedAvatar) {
        minimizedAvatar.textContent = callerName.charAt(0).toUpperCase();
    }

}

// Заполнить списки устройств в настройках
async function populateDeviceSelects() {
    await enumerateDevices();
    
    const audioInputSelect = document.getElementById('audioInputSelect');
    const audioOutputSelect = document.getElementById('audioOutputSelect');
    
    if (audioInputSelect && availableDevices.audioInputs) {
        audioInputSelect.innerHTML = '<option value="">По умолчанию</option>';
        availableDevices.audioInputs.forEach(device => {
            const option = document.createElement('option');
            option.value = device.deviceId;
            option.textContent = device.label || `Микрофон ${device.deviceId.substring(0, 8)}`;
            if (selectedAudioInputId === device.deviceId) {
                option.selected = true;
            }
            audioInputSelect.appendChild(option);
        });
    }
    
    if (audioOutputSelect && availableDevices.audioOutputs) {
        audioOutputSelect.innerHTML = '<option value="">По умолчанию</option>';
        availableDevices.audioOutputs.forEach(device => {
            const option = document.createElement('option');
            option.value = device.deviceId;
            option.textContent = device.label || `Устройство вывода ${device.deviceId.substring(0, 8)}`;
            if (selectedAudioOutputId === device.deviceId) {
                option.selected = true;
            }
            audioOutputSelect.appendChild(option);
        });
    }
    syncVolumeSliders();
}

// Минимизировать модальное окно звонка (вернуться к чату)
function minimizeCall() {
    const modal = document.getElementById('callModal');
    const minimizedCall = document.getElementById('minimizedCall');
    
    if (modal && minimizedCall) {
        modal.style.display = 'none';
        minimizedCall.style.display = 'flex';
        isCallMinimized = true;
    }
    scheduleCallControlsOffsetUpdate();
}

// Восстановить модальное окно звонка
function restoreCall() {
    const modal = document.getElementById('callModal');
    const minimizedCall = document.getElementById('minimizedCall');
    
    if (modal && minimizedCall) {
        modal.style.display = 'flex';
        minimizedCall.style.display = 'none';
        isCallMinimized = false;
    }
    scheduleCallControlsOffsetUpdate();
}

// Переключить отображение настроек звонка
function toggleCallSettings() {
    const settingsMenu = document.getElementById('callSettingsMenu');
    if (settingsMenu) {
        const isOpen = settingsMenu.classList.contains('open');
        if (isOpen) {
            settingsMenu.classList.remove('open');
        } else {
            settingsMenu.classList.add('open');
            populateDeviceSelects();
            syncVolumeSliders();
            updateInlineCallQualityControls();
        }
    }
}

// Проверка входящих звонков
let lastCheckedCallTime = Date.now();

async function checkIncomingCalls() {
    // Если уже идет звонок, не проверяем новые
    if (isCallActive) {
        return;
    }
    
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    
    try {
        // Проверяем через специальный endpoint для входящих звонков
        const response = await fetch(API_BASE + '/api/check-incoming-calls', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                token: token,
                last_check: lastCheckedCallTime
            })
        });
        
        if (response.ok) {
            const data = await response.json();
            
            if (data.success && data.call) {
                // Проверяем, что это новый звонок (не дубликат)
                if (data.call.id && data.call.id !== currentCallId) {
                    currentCallId = data.call.id;
                    lastCheckedCallTime = Date.now();
                    
                    handleIncomingCall({
                        id: data.call.id,
                        caller_id: data.call.caller_id,
                        caller_username: data.call.caller_username,
                        call_type: data.call.call_type || 'video'
                    });
                }
            }
        }
    } catch (error) {
        // Игнорируем ошибки, чтобы не спамить
        console.error('Error checking incoming calls:', error);
    }
}

// Запустить проверку входящих звонков
function startCallPolling() {
    // WebSocket обеспечивает сигналинг — поллинг не нужен
}

// Остановить проверку входящих звонков
function stopCallPolling() {
    if (callCheckInterval) {
        clearInterval(callCheckInterval);
        callCheckInterval = null;
    }
}

// Проверка ответа на исходящий звонок
async function checkCallResponse() {
    const token = localStorage.getItem('xipher_token');
    if (!token || !currentChat) return;
    
    try {
        const response = await fetch(API_BASE + '/api/check-call-response', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                token: token,
                receiver_id: currentChat.id
            })
        });
        
        if (response.ok) {
            const data = await response.json();
            
            if (data.success && data.response) {
                if (data.response === 'accepted') {
                    // Звонок принят - показываем активный звонок и запускаем таймер
                    showCallModal('active', currentCallType);
                    isCallActive = true;
                    
                    // Убеждаемся, что статус обновлен
                    const statusElement = document.getElementById('callStatus') || document.querySelector('.call-status');
                    if (statusElement) {
                        const callTypeText = currentCallType === 'video' ? 'Видеозвонок' : 'Голосовой звонок';
                        statusElement.textContent = callTypeText;
                    }
                    
                    if (!callStartTime) {
                        startCallTimer();
                    }
                    stopCallResponsePolling();
                } else if (data.response === 'rejected') {
                    // Звонок отклонен
                    notifications.info('Звонок отклонен');
                    endCall();
                }
            }
        }
    } catch (error) {
        console.error('Error checking call response:', error);
    }
}

// Запустить проверку ответа на звонок
function startCallResponsePolling() {
    startCallSignalingPolling();
}

// Остановить проверку ответа на звонок
function stopCallResponsePolling() {
    if (callResponseCheckInterval) {
        clearInterval(callResponseCheckInterval);
        callResponseCheckInterval = null;
    }
    stopCallSignalingPolling();
}

function startCallSignalingPolling() {
    if (!isCaller || isGroupCall || !currentChat) return;
    if (callAnswerPollInterval || callIcePollInterval) return;
    callAnswerPollInterval = setInterval(pollCallAnswerOnce, 700);
    callIcePollInterval = setInterval(pollCallIceOnce, 700);
}

function stopCallSignalingPolling() {
    if (callAnswerPollInterval) {
        clearInterval(callAnswerPollInterval);
        callAnswerPollInterval = null;
    }
    if (callIcePollInterval) {
        clearInterval(callIcePollInterval);
        callIcePollInterval = null;
    }
    lastCallIceCheck = 0;
}

async function pollCallAnswerOnce() {
    if (!peerConnection || !isCaller || isGroupCall || callEndSent || !currentChat) return;
    if (peerConnection.remoteDescription && peerConnection.remoteDescription.type) {
        stopCallSignalingPolling();
        return;
    }
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const response = await fetch(API_BASE + '/api/get-call-answer', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token: token,
                receiver_id: currentChat.id
            })
        });
        if (!response.ok) return;
        const data = await response.json();
        if (data.success && data.answer) {
            await handleCallAnswer({ answer: data.answer });
            stopCallSignalingPolling();
        }
    } catch (error) {
        console.error('[Call] pollCallAnswerOnce failed:', error);
    }
}

async function pollCallIceOnce() {
    if (!peerConnection || !isCaller || isGroupCall || callEndSent || !currentChat) return;
    const token = localStorage.getItem('xipher_token');
    if (!token) return;
    try {
        const payload = {
            token: token,
            receiver_id: currentChat.id
        };
        if (lastCallIceCheck > 0) {
            payload.last_check = String(lastCallIceCheck);
        }
        const response = await fetch(API_BASE + '/api/get-call-ice', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!response.ok) return;
        const data = await response.json();
        lastCallIceCheck = Math.floor(Date.now() / 1000);
        if (data.success && data.candidates) {
            let candidates = data.candidates;
            if (typeof candidates === 'string') {
                candidates = JSON.parse(candidates);
            }
            if (Array.isArray(candidates)) {
                for (const cand of candidates) {
                    await handleCallIceCandidate({ candidate: cand });
                }
            }
        }
    } catch (error) {
        console.error('[Call] pollCallIceOnce failed:', error);
    }
}

// Обработка входящего группового звонка
async function handleGroupCallOffer(data) {
    console.log('[GroupCall] Received group_call_offer:', data);
    
    const currentUserId = localStorage.getItem('xipher_user_id');
    if (!currentUserId || data.from_user_id === currentUserId) {
        return; // Игнорируем свои собственные предложения
    }
    
    // Проверяем, что это звонок в текущую группу
    if (currentChat && currentChat.id !== data.group_id) {
        return;
    }
    
    // Если это новый участник, создаем peer connection
    if (!peerConnections.has(data.from_user_id)) {
        await initGroupPeerConnection(data.from_user_id, false);
    }
    
    const pc = peerConnections.get(data.from_user_id);
    if (!pc) return;
    
    try {
        let offer = data.offer;
        if (typeof offer === 'string') {
            offer = JSON.parse(offer);
        }
        
        // Проверяем состояние перед обработкой offer
        const signalingState = pc.signalingState;
        console.log('[GroupCall] Signaling state before processing offer:', signalingState);
        
        if (signalingState !== 'stable') {
            console.warn('[GroupCall] Cannot process offer in state:', signalingState);
            return;
        }
        
        await pc.setRemoteDescription(new RTCSessionDescription(offer));
        
        // Проверяем состояние после setRemoteDescription
        if (pc.signalingState !== 'have-remote-offer') {
            console.warn('[GroupCall] Unexpected state after setRemoteDescription:', pc.signalingState);
            return;
        }
        
        // Создаем answer
        const rawAnswer = await pc.createAnswer();
        const answer = {
            type: rawAnswer.type,
            sdp: stripCodecs(rawAnswer.sdp || '')
        };
        await pc.setLocalDescription(answer);
        
        // Отправляем answer
        await sendGroupAnswer(data.from_user_id, answer);
    } catch (error) {
        console.error('[GroupCall] Ошибка обработки offer:', error);
    }
}

// Обработка группового answer
async function handleGroupCallAnswer(data) {
    console.log('[GroupCall] Received group_call_answer:', data);
    
    const currentUserId = localStorage.getItem('xipher_user_id');
    if (!currentUserId || data.from_user_id === currentUserId) {
        return;
    }
    
    const pc = peerConnections.get(data.from_user_id);
    if (!pc) return;
    
    try {
        let answer = data.answer;
        if (typeof answer === 'string') {
            answer = JSON.parse(answer);
        }
        await pc.setRemoteDescription(new RTCSessionDescription(answer));
    } catch (error) {
        console.error('[GroupCall] Ошибка обработки answer:', error);
    }
}

// Обработка группового ICE кандидата
async function handleGroupCallIceCandidate(data) {
    console.log('[GroupCall] Received group_call_ice_candidate:', data);
    
    const currentUserId = localStorage.getItem('xipher_user_id');
    if (!currentUserId || data.from_user_id === currentUserId) {
        return;
    }
    
    const pc = peerConnections.get(data.from_user_id);
    if (!pc) return;
    
    try {
        let candidate = data.candidate;
        if (typeof candidate === 'string') {
            candidate = JSON.parse(candidate);
        }
        await pc.addIceCandidate(new RTCIceCandidate(candidate));
    } catch (error) {
        console.error('[GroupCall] Ошибка обработки ICE кандидата:', error);
    }
}

// Обработка завершения группового звонка
function handleGroupCallEnd(data) {
    console.log('[GroupCall] Received group_call_end:', data);
    
    const userId = data.user_id || data.from_user_id;
    if (userId) {
        removeParticipant(userId);
    } else {
        // Если userId не указан, завершаем весь звонок
        endCall();
    }
}

// Обработка уведомления о групповом звонке
async function handleGroupCallNotification(data) {
    console.log('[GroupCall] Received group_call_notification:', data);
    
    const currentUserId = localStorage.getItem('xipher_user_id');
    if (!currentUserId) {
        console.log('[GroupCall] No user ID, ignoring');
        return;
    }
    
    // Если мы уже в звонке, игнорируем
    if (isCallActive) {
        console.log('[GroupCall] Already in call, ignoring');
        return;
    }
    
    // Проверяем, что это уведомление для нас
    // Проверяем как строку и число
    const userIdStr = currentUserId.toString();
    const userIdInt = parseInt(currentUserId);
    
    let isMember = false;
    if (data.members && Array.isArray(data.members)) {
        isMember = data.members.some(m => {
            const mStr = m.toString();
            const mInt = parseInt(m);
            return mStr === userIdStr || mInt === userIdInt || m === userIdStr || m === userIdInt;
        });
    }
    
    console.log('[GroupCall] Checking membership:', {
        userIdStr,
        userIdInt,
        members: data.members,
        isMember
    });
    
    if (!isMember) {
        console.log('[GroupCall] User not in members list, ignoring');
        return;
    }
    
    // Устанавливаем текущий чат как группу
    if (data.group_id) {
        // Находим группу в списке чатов
        let group = null;
        if (typeof chats !== 'undefined' && chats.length > 0) {
            group = chats.find(c => {
                const chatId = c.id ? c.id.toString() : '';
                const groupId = data.group_id ? data.group_id.toString() : '';
                return chatId === groupId && c.type === 'group';
            });
        }
        
        // Если не нашли в списке, создаем временный объект
        if (!group) {
            group = {
                id: data.group_id.toString(),
                name: data.group_name || data.group_id || 'Группа',
                type: 'group'
            };
        }
        
        currentChat = group;
        isGroupCall = true;
        isCaller = false; // Мы не инициатор звонка
        currentCallType = data.call_type || 'video';
        
        console.log('[GroupCall] Setting up incoming call for group:', group.name);
        
        // Убеждаемся, что модальное окно создано
        const modal = document.getElementById('callModal');
        if (!modal) {
            console.log('[GroupCall] Creating call modal');
            createCallModal();
        }
        
        // Обновляем имя и аватар в модальном окне
        const callUserName = document.getElementById('callUserName');
        const callAvatar = document.getElementById('callAvatar');
        if (callUserName) {
            callUserName.textContent = group.name;
        }
        if (callAvatar) {
            callAvatar.textContent = group.name.charAt(0).toUpperCase();
        }
        
        // Сохраняем данные для обработки при принятии
        window.pendingGroupCallNotification = data;

        const callerName = data.from_username || 'Участник';
        const callTypeText = currentCallType === 'video' ? 'видеозвонок' : 'звонок';
        const message = `Входящий групповой ${callTypeText} в ${group.name} от ${callerName}`;

        console.log('[GroupCall] Incoming group call received');
        showCallNotification(message, null, true, {
            group: group,
            callType: currentCallType,
            data: data
        });
        // Inline-панель не используется - новый fullscreen UI содержит все кнопки
        
        // Показываем встроенное окно группового звонка с кнопками
        try {
            let modal = document.getElementById('callModal');
            if (!modal) {
                createCallModal();
                modal = document.getElementById('callModal');
            }
            showGroupCallModal('incoming', currentCallType);
            if (modal) {
                modal.style.display = 'flex';
                modal.style.zIndex = '99999';
            }
        } catch (error) {
            console.error('[GroupCall] Error ensuring group call modal:', error);
        }
        
        // Запускаем звук звонка
        console.log('[GroupCall] Playing ringtone');
        playCallRingtone();
        
        // Дополнительная проверка - убеждаемся, что модальное окно видно
        setTimeout(() => {
            const modal = document.getElementById('callModal');
            if (modal) {
                const isVisible = window.getComputedStyle(modal).display !== 'none';
                console.log('[GroupCall] Modal visibility check:', isVisible, 'display:', window.getComputedStyle(modal).display);
                if (!isVisible) {
                    console.error('[GroupCall] Modal is not visible! Forcing display...');
                    modal.style.display = 'flex';
                    modal.style.zIndex = '99999';
                }
                
                // Проверяем видимость кнопок
                const incomingActions = document.getElementById('groupCallActionsIncoming');
                if (incomingActions) {
                    const actionsVisible = window.getComputedStyle(incomingActions).display !== 'none';
                    console.log('[GroupCall] Incoming actions visibility:', actionsVisible);
                    if (!actionsVisible) {
                        console.error('[GroupCall] Incoming actions not visible! Forcing display...');
                        incomingActions.style.display = 'flex';
                    }
                } else {
                    console.error('[GroupCall] Incoming actions element not found!');
                }
            } else {
                console.error('[GroupCall] Modal element not found after timeout!');
            }
        }, 200);
        
        console.log('[GroupCall] Incoming call notification processed successfully');
    } else {
        console.error('[GroupCall] No group_id in notification data');
    }
}

// Присоединиться к групповому звонку
async function joinGroupCall() {
    if (!currentChat || currentChat.type !== 'group') {
        notifications.warning('Выберите группу для присоединения к звонку');
        return;
    }
    
    if (!initWebRTC()) {
        return;
    }
    
    isGroupCall = true;
    currentCallType = 'video'; // По умолчанию видеозвонок
    pendingGroupSfuConfig = await fetchSfuRoomConfig(currentChat.id);
    
    try {
        cleanupMediaTracks(localStream, 'pre-joinGroupCall');
        localStream = null;
        await enumerateDevices();
        
        const allowGroupVideo = pendingGroupSfuConfig && currentCallType === 'video';
        const constraints = {
            audio: {
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true
            },
            video: allowGroupVideo ? buildCameraVideoConstraintsForCall() : false
        };
        
        localStream = await getUserMediaWithRecovery(constraints);
        if (!localStream) {
            notifications?.error && notifications.error('Не удалось получить доступ к камере/микрофону');
            endCall();
            return;
        }
        await enhanceLocalAudioStream(localStream, 'joinGroupCall');
        if (!pendingGroupSfuConfig) {
            stripGroupVideoTracks('group-no-sfu');
        }
        
        // Добавляем себя в список участников
        const currentUserId = localStorage.getItem('xipher_user_id');
        if (currentUserId) {
            callParticipants.add(currentUserId);
        }
        
        // Получаем список участников группы
        const groupMembers = await getGroupMembers(currentChat.id);
        
        // Показываем модальное окно группового звонка
        showGroupCallModal('active', currentCallType);
        isCallActive = true;
        startCallTimer();
        updateParticipantsCount();

        const hadSfuConfig = !!pendingGroupSfuConfig;
        const sfuStarted = await startGroupCallSfu(currentChat.id, currentCallType, pendingGroupSfuConfig);
        pendingGroupSfuConfig = null;
        if (sfuStarted) {
            console.log('[GroupCall] SFU session started on join');
            return;
        }
        if (hadSfuConfig) {
            stripGroupVideoTracks('sfu-fallback');
        }
        
        // Инициализируем соединения с каждым участником
        for (const member of groupMembers) {
            const memberId = member.user_id || member.id;
            if (memberId.toString() === currentUserId) {
                continue;
            }
            
            // Создаем peer connection
            await initGroupPeerConnection(memberId, true);
        }
        
    } catch (error) {
        console.error('[GroupCall] Ошибка присоединения к звонку:', error);
        notifications.error('Не удалось присоединиться к звонку');
        endCall();
    }
}

// --- Reactions & Polls helpers ---
async function addReaction(messageId, reaction) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return {success:false, error:'no token'};
    const resp = await fetch('/api/add-message-reaction', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({token, message_id: messageId, reaction})
    });
    return resp.json();
}

async function removeReaction(messageId, reaction) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return {success:false, error:'no token'};
    const resp = await fetch('/api/remove-message-reaction', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({token, message_id: messageId, reaction})
    });
    return resp.json();
}

async function getReactions(messageId) {
    const resp = await fetch('/api/get-message-reactions', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({message_id: messageId})
    });
    return resp.json();
}

async function createPoll(messageId, question, options, isAnonymous = true, allowsMultiple = false, closesAt = '') {
    const token = localStorage.getItem('xipher_token');
    if (!token) return {success:false, error:'no token'};
    const payload = {token, message_id: messageId, question, is_anonymous: isAnonymous, allows_multiple: allowsMultiple, closes_at: closesAt};
    options.forEach((opt, idx) => payload[`option${idx}`] = opt);
    const resp = await fetch('/api/create-poll', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify(payload)
    });
    return resp.json();
}

async function votePoll(pollId, optionId) {
    const token = localStorage.getItem('xipher_token');
    if (!token) return {success:false, error:'no token'};
    const resp = await fetch('/api/vote-poll', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({token, poll_id: pollId, option_id: optionId})
    });
    return resp.json();
}

// Экспорт функций для использования в других модулях
// Делаем это сразу при загрузке скрипта, чтобы функции были доступны
(function() {
    console.log('[Calls] Exporting functions to window object...');
window.startCall = startCall;
window.acceptCall = acceptCall;
window.rejectCall = rejectCall;
window.endCall = endCall;
window.handleIncomingCall = handleIncomingCall;
window.startCallPolling = startCallPolling;
window.stopCallPolling = stopCallPolling;
window.handleCallOffer = handleCallOffer;
window.handleCallAnswer = handleCallAnswer;
window.handleCallIceCandidate = handleCallIceCandidate;
window.handleCallEnd = handleCallEnd;
    window.handleGroupCallOffer = handleGroupCallOffer;
    window.handleGroupCallAnswer = handleGroupCallAnswer;
    window.handleGroupCallIceCandidate = handleGroupCallIceCandidate;
    window.handleGroupCallEnd = handleGroupCallEnd;
    window.handleGroupCallNotification = handleGroupCallNotification;
    window.joinGroupCall = joinGroupCall;
window.addReaction = addReaction;
window.removeReaction = removeReaction;
window.getReactions = getReactions;
window.createPoll = createPoll;
window.votePoll = votePoll;
    window.showCallNotification = showCallNotification;
    window.playCallRingtone = playCallRingtone;
    window.stopCallRingtone = stopCallRingtone;
    console.log('[Calls] ✅ Functions exported. handleCallOffer available:', typeof window.handleCallOffer === 'function');
    console.log('[Calls] ✅ showCallNotification available:', typeof window.showCallNotification === 'function');
})();
