const ERROR_PATTERNS = [
  /\bE \(\d+\) /,
  /\bESP_ERR_[A-Z0-9_]+\b/,
  /\b(?:error|failed|failure|fatal|panic|abort|assert|exception|timeout|timed out)\b/i,
  /Guru Meditation/i,
  /Backtrace:/i,
  /Core \d+ register dump/i,
  /rst:0x[0-9a-f]+/i,
  /ELF file SHA256/i
];

const WARNING_PATTERNS = [
  /\bW \(\d+\) .*?(?:failed|error|timeout|invalid|not found|unsupported)/i,
  /\bwarning:\b/i
];

const CONTINUATION_PATTERNS = [
  /^\s+0x[0-9a-fA-F]+/,
  /^\s*0x[0-9a-fA-F]+:/,
  /^\s*#[0-9]+\s+/,
  /^\s*at\s+/,
  /^\s*from\s+/,
  /^\s*\[[0-9]+\/[0-9]+\]/,
  /^\s*\^+$/,
  /compilation terminated/i,
  /Backtrace:/i
];

export function cleanMonitorLine(line) {
  return String(line || '')
    .replace(/\x1b\[[0-9;?]*[A-Za-z]/g, '')
    .trimEnd();
}

export function isMonitorDiagnosticLine(line) {
  const text = cleanMonitorLine(line).trim();
  if (!text) return false;
  return ERROR_PATTERNS.some((pattern) => pattern.test(text))
    || WARNING_PATTERNS.some((pattern) => pattern.test(text));
}

function isContinuationLine(line) {
  const text = cleanMonitorLine(line);
  return CONTINUATION_PATTERNS.some((pattern) => pattern.test(text));
}

export function extractMonitorDiagnostics(result = {}, options = {}) {
  const maxLines = Number(options.maxLines || 120);
  const rawLines = [
    ...String(result.stdout || '').split(/\r?\n/),
    ...String(result.stderr || '').split(/\r?\n/),
    String(result.error || '')
  ].map(cleanMonitorLine);

  const picked = [];
  const seen = new Set();

  function push(line) {
    const text = cleanMonitorLine(line);
    if (!text.trim() || seen.has(text)) return;
    seen.add(text);
    picked.push(text);
  }

  for (let index = 0; index < rawLines.length; index += 1) {
    const line = rawLines[index];
    if (!isMonitorDiagnosticLine(line)) {
      continue;
    }

    push(line);
    for (let lookahead = 1; lookahead <= 4; lookahead += 1) {
      const next = rawLines[index + lookahead];
      if (!next || !isContinuationLine(next)) break;
      push(next);
    }
  }

  const keyLines = picked.slice(0, maxLines);
  return {
    hasErrors: keyLines.length > 0,
    keyLines,
    text: keyLines.join('\n'),
    totalLines: rawLines.filter((line) => line.trim()).length,
    truncated: picked.length > keyLines.length || result.truncated === true
  };
}
