import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promises as fs } from 'node:fs';
import { spawn } from 'node:child_process';

import { loadConfig } from './lib/config.js';
import { callLlm, listProviderTemplates } from './lib/llm.js';
import { scanSdk } from './lib/sdkScanner.js';
import {
  buildChatPrompt,
  buildCodegenPrompt,
  buildPlanningPrompt,
  fallbackChat,
  fallbackCodegen,
  fallbackPlanning,
  normalizeAgentText,
  parseGeneratedFiles
} from './lib/workflows.js';
import { getPhysicalStatus } from './lib/physical.js';
import { loadSessions, saveSession } from './lib/storage.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const agentRoot = path.resolve(__dirname, '..');
const sdkRoot = path.resolve(agentRoot, '..');
const publicRoot = path.join(agentRoot, 'public');
const config = await loadConfig(agentRoot);

const contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon'
};

let sdkSummaryCache = null;

function sendJson(res, status, payload) {
  const body = JSON.stringify(payload, null, 2);
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store'
  });
  res.end(body);
}

function sendError(res, status, message, details = undefined) {
  sendJson(res, status, { ok: false, error: message, details });
}

async function readJson(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
  }
  if (chunks.length === 0) {
    return {};
  }
  const raw = Buffer.concat(chunks).toString('utf8');
  if (!raw.trim()) {
    return {};
  }
  return JSON.parse(raw);
}

function safeProjectPath(projectPath) {
  if (typeof projectPath !== 'string' || projectPath.trim() === '') {
    throw new Error('projectPath is required');
  }

  const normalized = projectPath.replaceAll('\\', '/').replace(/^\/+/, '');
  if (!normalized.startsWith('project/')) {
    throw new Error('projectPath must be under project/');
  }

  const absolute = path.resolve(sdkRoot, normalized);
  const projectRoot = path.resolve(sdkRoot, 'project');
  if (absolute !== projectRoot && !absolute.startsWith(projectRoot + path.sep)) {
    throw new Error('projectPath escapes project/');
  }
  return { normalized, absolute };
}

function safeOutputFile(filePath) {
  if (typeof filePath !== 'string' || filePath.trim() === '') {
    throw new Error('file path is required');
  }

  const normalized = filePath.replaceAll('\\', '/').replace(/^\/+/, '');
  if (!normalized.startsWith('project/')) {
    throw new Error(`refusing to write outside project/: ${filePath}`);
  }

  const absolute = path.resolve(sdkRoot, normalized);
  const projectRoot = path.resolve(sdkRoot, 'project');
  if (absolute !== projectRoot && !absolute.startsWith(projectRoot + path.sep)) {
    throw new Error(`path escapes project/: ${filePath}`);
  }

  return { normalized, absolute };
}

async function getSdkSummary() {
  if (!sdkSummaryCache) {
    sdkSummaryCache = await scanSdk(sdkRoot);
  }
  return sdkSummaryCache;
}

async function serveStatic(req, res, pathname) {
  const requested = pathname === '/' ? '/index.html' : pathname;
  const absolute = path.resolve(publicRoot, requested.slice(1));

  if (absolute !== publicRoot && !absolute.startsWith(publicRoot + path.sep)) {
    sendError(res, 403, 'forbidden');
    return;
  }

  try {
    const stat = await fs.stat(absolute);
    if (!stat.isFile()) {
      sendError(res, 404, 'not found');
      return;
    }

    const ext = path.extname(absolute);
    res.writeHead(200, {
      'content-type': contentTypes[ext] || 'application/octet-stream',
      'cache-control': 'no-cache'
    });
    res.end(await fs.readFile(absolute));
  } catch (error) {
    if (error.code === 'ENOENT') {
      sendError(res, 404, 'not found');
      return;
    }
    sendError(res, 500, 'failed to serve asset', error.message);
  }
}

async function handlePlan(req, res) {
  const body = await readJson(req);
  const sdkSummary = await getSdkSummary();
  const prompt = buildPlanningPrompt({
    requirement: body.requirement || '',
    sdkSummary,
    skillFocus: body.skillFocus || 'skillset-1'
  });

  if (body.useLlm) {
    try {
      const text = await callLlm({
        config,
        providerRequest: body.provider || {},
        messages: prompt.messages,
        temperature: body.temperature ?? 0.2
      });
      sendJson(res, 200, {
        ok: true,
        mode: 'llm',
        result: normalizeAgentText(text),
        sdkSummary
      });
      return;
    } catch (error) {
      const fallback = fallbackPlanning(body.requirement || '', sdkSummary);
      sendJson(res, 200, {
        ok: true,
        mode: 'fallback',
        warning: error.message,
        result: fallback,
        sdkSummary
      });
      return;
    }
  }

  sendJson(res, 200, {
    ok: true,
    mode: 'fallback',
    result: fallbackPlanning(body.requirement || '', sdkSummary),
    sdkSummary
  });
}

async function handleChat(req, res) {
  const body = await readJson(req);
  const sdkSummary = await getSdkSummary();
  const prompt = buildChatPrompt({
    question: body.question || '',
    requirement: body.requirement || '',
    currentPlan: body.currentPlan || '',
    sdkSummary
  });

  if (body.useLlm) {
    try {
      const text = await callLlm({
        config,
        providerRequest: body.provider || {},
        messages: prompt.messages,
        temperature: body.temperature ?? 0.2
      });
      sendJson(res, 200, {
        ok: true,
        mode: 'llm',
        answer: normalizeAgentText(text)
      });
      return;
    } catch (error) {
      sendJson(res, 200, {
        ok: true,
        mode: 'fallback',
        warning: error.message,
        answer: fallbackChat(body.question || '', sdkSummary)
      });
      return;
    }
  }

  sendJson(res, 200, {
    ok: true,
    mode: 'fallback',
    answer: fallbackChat(body.question || '', sdkSummary)
  });
}

async function handleCodegen(req, res) {
  const body = await readJson(req);
  const sdkSummary = await getSdkSummary();
  const prompt = buildCodegenPrompt({
    requirement: body.requirement || '',
    projectName: body.projectName || '',
    plan: body.plan || '',
    sdkSummary
  });

  if (body.useLlm) {
    try {
      const text = await callLlm({
        config,
        providerRequest: body.provider || {},
        messages: prompt.messages,
        temperature: body.temperature ?? 0.15
      });
      const fallback = fallbackCodegen(body.requirement || '', body.projectName || '', sdkSummary);
      const parsedFiles = parseGeneratedFiles(text);
      sendJson(res, 200, {
        ok: true,
        mode: 'llm',
        projectName: fallback.projectName,
        result: normalizeAgentText(text),
        files: parsedFiles.length > 0 ? parsedFiles : fallback.files,
        note: parsedFiles.length > 0
          ? 'LLM file blocks were parsed into editable files.'
          : 'No parsable LLM file blocks were found. Fallback files are included as a safe editable scaffold.'
      });
      return;
    } catch (error) {
      const fallback = fallbackCodegen(body.requirement || '', body.projectName || '', sdkSummary);
      sendJson(res, 200, {
        ok: true,
        mode: 'fallback',
        warning: error.message,
        ...fallback
      });
      return;
    }
  }

  sendJson(res, 200, {
    ok: true,
    mode: 'fallback',
    ...fallbackCodegen(body.requirement || '', body.projectName || '', sdkSummary)
  });
}

async function handleProjectWrite(req, res) {
  const body = await readJson(req);
  const files = Array.isArray(body.files) ? body.files : [];
  if (files.length === 0) {
    sendError(res, 400, 'files must be a non-empty array');
    return;
  }

  const written = [];
  try {
    for (const file of files) {
      const { normalized, absolute } = safeOutputFile(file.path);
      await fs.mkdir(path.dirname(absolute), { recursive: true });
      await fs.writeFile(absolute, String(file.content ?? ''), 'utf8');
      written.push(normalized);
    }
  } catch (error) {
    sendError(res, 400, error.message);
    return;
  }

  sendJson(res, 200, { ok: true, written });
}

function runTool(tool, args, cwd, timeoutMs = 0) {
  return new Promise((resolve) => {
    const startedAt = new Date().toISOString();
    const child = spawn(tool, args, {
      cwd,
      shell: false,
      env: process.env
    });

    let stdout = '';
    let stderr = '';
    let timedOut = false;
    let timer = null;
    if (timeoutMs > 0) {
      timer = setTimeout(() => {
        timedOut = true;
        child.kill('SIGTERM');
      }, timeoutMs);
    }
    child.stdout?.on('data', (chunk) => {
      stdout += chunk.toString();
    });
    child.stderr?.on('data', (chunk) => {
      stderr += chunk.toString();
    });
    child.on('error', (error) => {
      if (timer) clearTimeout(timer);
      resolve({ ok: false, code: -1, startedAt, stdout, stderr, error: error.message });
    });
    child.on('close', (code) => {
      if (timer) clearTimeout(timer);
      resolve({ ok: code === 0 || timedOut, code, timedOut, startedAt, finishedAt: new Date().toISOString(), stdout, stderr });
    });
  });
}

function bashPath(filePath) {
  return filePath.replaceAll('\\', '/');
}

async function handleTool(req, res, kind) {
  const body = await readJson(req);
  const { normalized } = safeProjectPath(body.projectPath || 'project/led_effects_demo');
  const target = body.target || 'esp32';
  const board = body.board || `my_board_${target}`;
  const port = body.port || '/dev/ttyUSB0';
  const script = path.join(sdkRoot, 'tools', `${kind}.sh`);
  const args = [bashPath(script), bashPath(path.join(sdkRoot, normalized))];

  if (kind === 'build') {
    args.push(target, board);
  } else {
    args.push(port);
  }

  const timeoutMs = kind === 'monitor' ? Number(body.timeoutMs || 15000) : 0;
  const result = await runTool('bash', args, sdkRoot, timeoutMs);
  sendJson(res, 200, {
    ok: result.ok,
    tool: kind,
    command: ['bash', ...args].join(' '),
    result
  });
}

async function handleApi(req, res, pathname) {
  try {
    if (req.method === 'GET' && pathname === '/api/health') {
      sendJson(res, 200, {
        ok: true,
        version: '0.1.0',
        sdkRoot,
        agentRoot,
        providers: listProviderTemplates(config),
        physical: getPhysicalStatus()
      });
      return;
    }

    if (req.method === 'GET' && pathname === '/api/sdk/summary') {
      sendJson(res, 200, { ok: true, summary: await getSdkSummary() });
      return;
    }

    if (req.method === 'POST' && pathname === '/api/sdk/refresh') {
      sdkSummaryCache = await scanSdk(sdkRoot);
      sendJson(res, 200, { ok: true, summary: sdkSummaryCache });
      return;
    }

    if (req.method === 'GET' && pathname === '/api/sessions') {
      sendJson(res, 200, { ok: true, sessions: await loadSessions(agentRoot) });
      return;
    }

    if (req.method === 'POST' && pathname === '/api/sessions') {
      const body = await readJson(req);
      const session = await saveSession(agentRoot, body);
      sendJson(res, 200, { ok: true, session });
      return;
    }

    if (req.method === 'POST' && pathname === '/api/agent/plan') {
      await handlePlan(req, res);
      return;
    }

    if (req.method === 'POST' && pathname === '/api/agent/chat') {
      await handleChat(req, res);
      return;
    }

    if (req.method === 'POST' && pathname === '/api/agent/codegen') {
      await handleCodegen(req, res);
      return;
    }

    if (req.method === 'POST' && pathname === '/api/project/write') {
      await handleProjectWrite(req, res);
      return;
    }

    if (req.method === 'POST' && pathname === '/api/tools/build') {
      await handleTool(req, res, 'build');
      return;
    }

    if (req.method === 'POST' && pathname === '/api/tools/flash') {
      await handleTool(req, res, 'flash');
      return;
    }

    if (req.method === 'POST' && pathname === '/api/tools/monitor') {
      await handleTool(req, res, 'monitor');
      return;
    }

    if (req.method === 'GET' && pathname === '/api/physical/status') {
      sendJson(res, 200, { ok: true, physical: getPhysicalStatus() });
      return;
    }

    sendError(res, 404, 'api route not found');
  } catch (error) {
    sendError(res, 500, error.message, error.stack);
  }
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = decodeURIComponent(url.pathname);

  if (pathname.startsWith('/api/')) {
    await handleApi(req, res, pathname);
    return;
  }

  await serveStatic(req, res, pathname);
});

server.listen(config.port, config.host, () => {
  console.log(`Moce Hardware Agent running at http://${config.host}:${config.port}`);
  console.log(`SDK root: ${sdkRoot}`);
});
