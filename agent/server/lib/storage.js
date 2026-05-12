import path from 'node:path';
import { promises as fs } from 'node:fs';

async function ensureDataDir(agentRoot) {
  const dataDir = path.join(agentRoot, 'data');
  await fs.mkdir(dataDir, { recursive: true });
  return dataDir;
}

async function readJsonSafe(filePath, fallback) {
  try {
    return JSON.parse(await fs.readFile(filePath, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') {
      return fallback;
    }
    throw error;
  }
}

export async function loadSessions(agentRoot) {
  const dataDir = await ensureDataDir(agentRoot);
  return readJsonSafe(path.join(dataDir, 'sessions.json'), []);
}

export async function saveSession(agentRoot, sessionInput) {
  const dataDir = await ensureDataDir(agentRoot);
  const filePath = path.join(dataDir, 'sessions.json');
  const sessions = await readJsonSafe(filePath, []);
  const now = new Date().toISOString();
  const session = {
    id: sessionInput.id || `session_${Date.now()}`,
    title: sessionInput.title || 'Untitled hardware agent session',
    requirement: sessionInput.requirement || '',
    plan: sessionInput.plan || '',
    files: Array.isArray(sessionInput.files) ? sessionInput.files : [],
    updatedAt: now,
    createdAt: sessionInput.createdAt || now
  };

  const index = sessions.findIndex((item) => item.id === session.id);
  if (index >= 0) {
    sessions[index] = session;
  } else {
    sessions.unshift(session);
  }
  await fs.writeFile(filePath, JSON.stringify(sessions.slice(0, 50), null, 2), 'utf8');
  return session;
}
