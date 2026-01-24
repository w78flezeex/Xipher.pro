'use strict';

const express = require('express');
const cors = require('cors');
const rateLimit = require('express-rate-limit');
const RedisStore = require('rate-limit-redis');
const crypto = require('crypto');
const redis = require('redis');
const { promisify } = require('util');

const SECRET_KEY = 'xipher-svortex-2026-supersecret';
const TURN_URLS = [
  'turn:turn.xipher.pro:3478?transport=udp',
  'turn:turn.xipher.pro:3478?transport=tcp',
  'turns:turn.xipher.pro:5349?transport=tcp'
];

const PORT = Number.parseInt(process.env.PORT || '8080', 10);
const REDIS_URL = process.env.REDIS_URL || 'redis://127.0.0.1:6379';
const DEFAULT_TTL_MINUTES = 60;
const MAX_TTL_MINUTES = 24 * 60;
const USER_ID_MAX_LEN = 128;
const ALLOWED_ORIGINS = (process.env.CORS_ORIGINS || 'https://xipher.pro,https://www.xipher.pro,http://localhost:3000')
  .split(',')
  .map((origin) => origin.trim())
  .filter(Boolean);

const app = express();
app.set('trust proxy', 1);
app.disable('x-powered-by');
app.use(express.json({ limit: '32kb' }));
app.use((req, res, next) => {
  res.setHeader('Cache-Control', 'no-store');
  next();
});

const corsOptions = {
  origin(origin, callback) {
    if (!origin) return callback(null, true);
    if (ALLOWED_ORIGINS.includes(origin)) return callback(null, true);
    return callback(new Error('CORS not allowed'));
  },
  credentials: true,
  methods: ['POST', 'OPTIONS'],
  allowedHeaders: ['Content-Type', 'Authorization']
};

app.use(cors(corsOptions));
app.options('/api/turn-credentials', cors(corsOptions));

const redisClient = redis.createClient(REDIS_URL);
let redisReady = false;
redisClient.on('ready', () => {
  redisReady = true;
  console.log('[redis] ready');
});
redisClient.on('end', () => {
  redisReady = false;
  console.error('[redis] connection closed');
});
redisClient.on('reconnecting', () => {
  redisReady = false;
  console.warn('[redis] reconnecting');
});
redisClient.on('error', (err) => {
  redisReady = false;
  console.error('[redis] connection error:', err);
});

const turnLimiter = rateLimit({
  windowMs: 60 * 1000,
  max: 5,
  standardHeaders: true,
  legacyHeaders: false,
  keyGenerator: (req) => {
    const userId = req.body && typeof req.body.userId === 'string' ? req.body.userId.trim() : '';
    return userId || req.ip;
  },
  store: new RedisStore({ client: redisClient }),
  handler: (req, res) => {
    res.status(429).json({
      error: 'rate_limit',
      message: 'Too many requests, try again later'
    });
  }
});

const redisSetAsync = promisify(redisClient.set).bind(redisClient);
const redisQuitAsync = promisify(redisClient.quit).bind(redisClient);

function ensureRedisReady(req, res, next) {
  if (!redisReady) {
    return res.status(503).json({
      error: 'redis_unavailable',
      message: 'Temporary service issue'
    });
  }
  return next();
}

function parseTtlMinutes(raw) {
  if (raw === undefined || raw === null || raw === '') return DEFAULT_TTL_MINUTES;
  const parsed = Number(raw);
  if (!Number.isFinite(parsed)) return DEFAULT_TTL_MINUTES;
  const floored = Math.floor(parsed);
  if (floored <= 0) return DEFAULT_TTL_MINUTES;
  return Math.min(floored, MAX_TTL_MINUTES);
}

app.post('/api/turn-credentials', ensureRedisReady, turnLimiter, async (req, res) => {
  const userId = req.body && typeof req.body.userId === 'string' ? req.body.userId.trim() : '';
  const ttlMinutes = parseTtlMinutes(req.body ? req.body.ttlMinutes : undefined);
  const requestTs = new Date().toISOString();

  console.info(`[TURN] request userId=${userId || 'missing'} ts=${requestTs}`);

  if (!userId) {
    return res.status(400).json({
      error: 'bad_request',
      message: 'userId is required'
    });
  }

  if (userId.length > USER_ID_MAX_LEN) {
    return res.status(400).json({
      error: 'bad_request',
      message: 'userId is too long'
    });
  }

  const ttlSeconds = ttlMinutes * 60;
  const expires = Math.floor(Date.now() / 1000) + ttlSeconds;
  const username = `${userId}:${expires}`;
  const credential = crypto.createHmac('sha1', SECRET_KEY).update(username).digest('base64');

  try {
    const key = `turn:${username}`;
    const value = JSON.stringify({ userId, expires });
    await redisSetAsync(key, value, 'EX', ttlSeconds);
  } catch (err) {
    console.error('[TURN] Redis write failed:', err);
    return res.status(503).json({
      error: 'redis_unavailable',
      message: 'Temporary service issue'
    });
  }

  return res.json({
    urls: TURN_URLS,
    username,
    credential,
    expires
  });
});

app.use((err, req, res, next) => {
  if (err && err.type === 'entity.parse.failed') {
    return res.status(400).json({
      error: 'bad_request',
      message: 'Invalid JSON payload'
    });
  }
  if (err && err.message === 'CORS not allowed') {
    return res.status(403).json({
      error: 'cors',
      message: 'Origin not allowed'
    });
  }
  console.error('[turn-service] unhandled error:', err);
  return res.status(500).json({
    error: 'server_error',
    message: 'Internal server error'
  });
});

app.get('/healthz', (req, res) => {
  res.json({ ok: true, redis: redisReady });
});

app.get('/readyz', (req, res) => {
  if (!redisReady) {
    return res.status(503).json({ ok: false, redis: false });
  }
  return res.json({ ok: true, redis: true });
});

let server;
async function start() {
  server = app.listen(PORT, () => {
    console.log(`[turn-service] listening on :${PORT}`);
  });
  server.keepAliveTimeout = 65 * 1000;
  server.headersTimeout = 70 * 1000;
}

start().catch((err) => {
  console.error('[turn-service] startup failed:', err);
  process.exit(1);
});

async function shutdown(signal) {
  console.log(`[turn-service] shutting down (${signal})`);
  try {
    await redisQuitAsync();
  } catch (err) {
    console.error('[turn-service] redis shutdown error:', err);
  }
  if (server) {
    server.close(() => process.exit(0));
  } else {
    process.exit(0);
  }
}

process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('unhandledRejection', (err) => {
  console.error('[turn-service] unhandled rejection:', err);
});
process.on('uncaughtException', (err) => {
  console.error('[turn-service] uncaught exception:', err);
});
