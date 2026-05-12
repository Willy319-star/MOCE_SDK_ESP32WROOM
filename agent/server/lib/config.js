import path from 'node:path';
import { promises as fs } from 'node:fs';

const defaultConfig = {
  host: process.env.MOCE_AGENT_HOST || '127.0.0.1',
  port: Number(process.env.MOCE_AGENT_PORT || 4173),
  defaultProvider: 'openai',
  providers: {
    openai: {
      kind: 'openai-compatible',
      baseUrl: 'https://api.openai.com/v1',
      apiKeyEnv: 'OPENAI_API_KEY',
      model: process.env.OPENAI_MODEL || 'gpt-4.1'
    },
    deepseek: {
      kind: 'openai-compatible',
      baseUrl: 'https://api.deepseek.com/v1',
      apiKeyEnv: 'DEEPSEEK_API_KEY',
      model: process.env.DEEPSEEK_MODEL || 'deepseek-chat'
    },
    openrouter: {
      kind: 'openai-compatible',
      baseUrl: 'https://openrouter.ai/api/v1',
      apiKeyEnv: 'OPENROUTER_API_KEY',
      model: process.env.OPENROUTER_MODEL || 'openai/gpt-4.1'
    },
    anthropic: {
      kind: 'anthropic',
      baseUrl: 'https://api.anthropic.com',
      apiKeyEnv: 'ANTHROPIC_API_KEY',
      model: process.env.ANTHROPIC_MODEL || 'claude-3-7-sonnet-latest'
    },
    gemini: {
      kind: 'gemini',
      baseUrl: 'https://generativelanguage.googleapis.com',
      apiKeyEnv: 'GEMINI_API_KEY',
      model: process.env.GEMINI_MODEL || 'gemini-2.5-pro'
    },
    ollama: {
      kind: 'ollama',
      baseUrl: 'http://127.0.0.1:11434',
      model: process.env.OLLAMA_MODEL || 'qwen2.5-coder:14b'
    },
    custom: {
      kind: 'openai-compatible',
      baseUrl: process.env.CUSTOM_LLM_BASE_URL || 'http://127.0.0.1:8000/v1',
      apiKeyEnv: 'CUSTOM_LLM_API_KEY',
      model: process.env.CUSTOM_LLM_MODEL || 'local-model'
    }
  }
};

function mergeConfig(base, override) {
  return {
    ...base,
    ...override,
    providers: {
      ...base.providers,
      ...(override.providers || {})
    }
  };
}

export async function loadConfig(agentRoot) {
  const configPath = path.join(agentRoot, 'agent.config.json');
  try {
    const raw = await fs.readFile(configPath, 'utf8');
    return mergeConfig(defaultConfig, JSON.parse(raw));
  } catch (error) {
    if (error.code === 'ENOENT') {
      return defaultConfig;
    }
    throw error;
  }
}
