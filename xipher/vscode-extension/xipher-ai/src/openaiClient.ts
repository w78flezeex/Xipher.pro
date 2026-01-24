import * as http from 'http';
import * as https from 'https';
import { URL } from 'url';

export type ChatMessage = {
  role: 'system' | 'user' | 'assistant';
  content: string;
};

type ChatRequest = {
  apiBaseUrl: string;
  token: string;
  model: string;
  messages: ChatMessage[];
  timeoutMs: number;
};

type OpenAiResponse = {
  choices?: Array<{ message?: { content?: string } }>;
  error?: { message?: string };
};

export async function createChatCompletion(request: ChatRequest): Promise<string> {
  const url = new URL(request.apiBaseUrl.replace(/\/$/, '') + '/chat/completions');
  const payload = JSON.stringify({
    model: request.model,
    messages: request.messages
  });

  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    'Content-Length': Buffer.byteLength(payload).toString(),
    Authorization: `Bearer ${request.token}`
  };

  const responseText = await sendRequest(url, payload, headers, request.timeoutMs);
  let data: OpenAiResponse;

  try {
    data = JSON.parse(responseText) as OpenAiResponse;
  } catch (error) {
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

function sendRequest(url: URL, body: string, headers: Record<string, string>, timeoutMs: number): Promise<string> {
  const isHttps = url.protocol === 'https:';
  const transport = isHttps ? https : http;

  return new Promise((resolve, reject) => {
    const request = transport.request(
      {
        method: 'POST',
        hostname: url.hostname,
        port: url.port ? Number(url.port) : isHttps ? 443 : 80,
        path: url.pathname + url.search,
        headers,
        timeout: timeoutMs
      },
      (res: http.IncomingMessage) => {
        const chunks: Buffer[] = [];
        res.on('data', (chunk: Buffer | string) =>
          chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk))
        );
        res.on('end', () => {
          const text = Buffer.concat(chunks).toString('utf8');
          if (res.statusCode && res.statusCode >= 400) {
            reject(new Error(`API error ${res.statusCode}: ${text}`));
            return;
          }
          resolve(text);
        });
      }
    );

    request.on('error', (error: Error) => reject(error));
    request.on('timeout', () => {
      request.destroy();
      reject(new Error('Request timeout.'));
    });

    request.write(body);
    request.end();
  });
}
