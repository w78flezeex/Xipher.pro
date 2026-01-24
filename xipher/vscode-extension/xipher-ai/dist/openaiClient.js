"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.createChatCompletion = void 0;
const http = __importStar(require("http"));
const https = __importStar(require("https"));
const url_1 = require("url");
async function createChatCompletion(request) {
    const url = new url_1.URL(request.apiBaseUrl.replace(/\/$/, '') + '/chat/completions');
    const payload = JSON.stringify({
        model: request.model,
        messages: request.messages
    });
    const headers = {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload).toString(),
        Authorization: `Bearer ${request.token}`
    };
    const responseText = await sendRequest(url, payload, headers, request.timeoutMs);
    let data;
    try {
        data = JSON.parse(responseText);
    }
    catch (error) {
        throw new Error('Invalid response from API.');
    }
    if (data.error?.message) {
        throw new Error(data.error.message);
    }
    const content = data.choices?.[0]?.message?.content;
    if (!content) {
        throw new Error('Empty response from API.');
    }
    return content.trim();
}
exports.createChatCompletion = createChatCompletion;
function sendRequest(url, body, headers, timeoutMs) {
    const isHttps = url.protocol === 'https:';
    const transport = isHttps ? https : http;
    return new Promise((resolve, reject) => {
        const request = transport.request({
            method: 'POST',
            hostname: url.hostname,
            port: url.port ? Number(url.port) : isHttps ? 443 : 80,
            path: url.pathname + url.search,
            headers,
            timeout: timeoutMs
        }, (res) => {
            const chunks = [];
            res.on('data', (chunk) => chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk)));
            res.on('end', () => {
                const text = Buffer.concat(chunks).toString('utf8');
                if (res.statusCode && res.statusCode >= 400) {
                    reject(new Error(`API error ${res.statusCode}: ${text}`));
                    return;
                }
                resolve(text);
            });
        });
        request.on('error', (error) => reject(error));
        request.on('timeout', () => {
            request.destroy();
            reject(new Error('Request timeout.'));
        });
        request.write(body);
        request.end();
    });
}
//# sourceMappingURL=openaiClient.js.map