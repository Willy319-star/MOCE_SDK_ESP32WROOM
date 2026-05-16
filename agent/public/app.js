const state = {
  health: null,
  sdk: null,
  plan: '',
  files: [],
  validation: null,
  selectedFilePath: '',
  currentSessionId: null,
  activity: [],
  prompts: []
};

const steps = [
  '提出产品需求',
  '拆解任务',
  '器件选型',
  '硬件资源规划',
  '硬件搭建',
  '嵌入式开发',
  '测试调试',
  '接入 physical',
  '结构设计装配',
  '整机联调',
  'physical vibe code',
  '电路板量产设计'
];

const hardwareKeywords = [
  ['ToF', /tof|vl53|测距|distance/i],
  ['OLED', /oled|display|显示|屏/i],
  ['Servo', /servo|舵机|steer|scan/i],
  ['Button', /button|按键|按钮/i],
  ['WiFi', /wifi|tcp|udp|上位机|host/i],
  ['LED', /led|灯/i]
];

const $ = (selector) => document.querySelector(selector);

function escapeHtml(value) {
  return String(value || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function escapeAttr(value) {
  return escapeHtml(value);
}

function renderInlineMarkdown(value) {
  return escapeHtml(value)
    .replace(/`([^`]+)`/g, '<code>$1</code>')
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/__([^_]+)__/g, '<strong>$1</strong>')
    .replace(/\*([^*\n]+)\*/g, '<em>$1</em>')
    .replace(/_([^_\n]+)_/g, '<em>$1</em>')
    .replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noreferrer">$1</a>');
}

function isMarkdownTableStart(lines, index) {
  return index + 1 < lines.length
    && lines[index].includes('|')
    && /^\s*\|?\s*:?-{3,}/.test(lines[index + 1]);
}

function renderMarkdownTable(tableLines) {
  const rows = tableLines
    .filter((line, index) => index !== 1)
    .map((line) => line.trim().replace(/^\|/, '').replace(/\|$/, '').split('|').map((cell) => cell.trim()));
  if (rows.length === 0) return '';
  const head = rows[0];
  const body = rows.slice(1);
  return `
    <table>
      <thead><tr>${head.map((cell) => `<th>${renderInlineMarkdown(cell)}</th>`).join('')}</tr></thead>
      <tbody>${body.map((row) => `<tr>${row.map((cell) => `<td>${renderInlineMarkdown(cell)}</td>`).join('')}</tr>`).join('')}</tbody>
    </table>
  `;
}

function markdownToHtml(markdown) {
  const lines = String(markdown || '').replace(/\r\n/g, '\n').split('\n');
  const html = [];
  let index = 0;

  function isBlockStart(line, cursor) {
    return !line.trim()
      || /^#{1,6}\s+/.test(line)
      || /^```/.test(line)
      || /^>\s?/.test(line)
      || /^\s*[-*+]\s+/.test(line)
      || /^\s*\d+\.\s+/.test(line)
      || isMarkdownTableStart(lines, cursor);
  }

  while (index < lines.length) {
    const line = lines[index];
    if (!line.trim()) {
      index += 1;
      continue;
    }

    const fence = line.match(/^```\s*([A-Za-z0-9_-]*)\s*$/);
    if (fence) {
      const language = fence[1] || 'text';
      const codeLines = [];
      index += 1;
      while (index < lines.length && !/^```\s*$/.test(lines[index])) {
        codeLines.push(lines[index]);
        index += 1;
      }
      if (index < lines.length) index += 1;
      html.push(`<pre class="code-block language-${escapeAttr(language)}"><code>${escapeHtml(codeLines.join('\n'))}</code></pre>`);
      continue;
    }

    if (isMarkdownTableStart(lines, index)) {
      const tableLines = [lines[index], lines[index + 1]];
      index += 2;
      while (index < lines.length && lines[index].includes('|')) {
        tableLines.push(lines[index]);
        index += 1;
      }
      html.push(renderMarkdownTable(tableLines));
      continue;
    }

    const heading = line.match(/^(#{1,6})\s+(.+)$/);
    if (heading) {
      const level = Math.min(heading[1].length, 6);
      html.push(`<h${level}>${renderInlineMarkdown(heading[2].trim())}</h${level}>`);
      index += 1;
      continue;
    }

    if (/^>\s?/.test(line)) {
      const quoteLines = [];
      while (index < lines.length && /^>\s?/.test(lines[index])) {
        quoteLines.push(lines[index].replace(/^>\s?/, ''));
        index += 1;
      }
      html.push(`<blockquote>${markdownToHtml(quoteLines.join('\n'))}</blockquote>`);
      continue;
    }

    const unordered = line.match(/^\s*[-*+]\s+(.+)$/);
    const ordered = line.match(/^\s*\d+\.\s+(.+)$/);
    if (unordered || ordered) {
      const tag = unordered ? 'ul' : 'ol';
      const items = [];
      const itemPattern = unordered ? /^\s*[-*+]\s+(.+)$/ : /^\s*\d+\.\s+(.+)$/;
      while (index < lines.length) {
        const item = lines[index].match(itemPattern);
        if (!item) break;
        items.push(`<li>${renderInlineMarkdown(item[1].trim())}</li>`);
        index += 1;
      }
      html.push(`<${tag}>${items.join('')}</${tag}>`);
      continue;
    }

    const paragraph = [line.trim()];
    index += 1;
    while (index < lines.length && !isBlockStart(lines[index], index)) {
      paragraph.push(lines[index].trim());
      index += 1;
    }
    html.push(`<p>${renderInlineMarkdown(paragraph.join(' '))}</p>`);
  }

  return html.join('');
}

function setStatus(text, kind = '') {
  const el = $('#healthLine');
  el.className = kind ? `status-${kind}` : '';
  el.textContent = text;
}

function logActivity(message, kind = '') {
  const at = new Date().toLocaleTimeString();
  state.activity.unshift({ at, message, kind });
  state.activity = state.activity.slice(0, 40);
  $('#logCount').textContent = String(state.activity.length);
  $('#activityLog').innerHTML = state.activity.map((entry) => `
    <div class="log-entry ${entry.kind}">
      <strong>${entry.at}</strong> ${escapeHtml(entry.message)}
    </div>
  `).join('');
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: {
      'content-type': 'application/json',
      ...(options.headers || {})
    },
    ...options,
    body: options.body ? JSON.stringify(options.body) : undefined
  });
  const data = await response.json();
  if (!response.ok || data.ok === false) {
    throw new Error(data.error || response.statusText);
  }
  return data;
}

function providerRequest() {
  const selected = state.health?.providers?.find((item) => item.name === $('#providerName').value);
  return {
    name: $('#providerName').value,
    kind: selected?.kind || 'openai-compatible',
    model: $('#modelName').value.trim(),
    baseUrl: $('#baseUrl').value.trim(),
    apiKey: $('#apiKey').value.trim()
  };
}

function currentPayload() {
  return {
    requirement: $('#requirement').value.trim(),
    promptPath: $('#promptPath')?.value || '',
    projectName: $('#projectName').value.trim(),
    target: $('#targetName').value.trim(),
    board: $('#boardName').value.trim(),
    port: $('#portName').value.trim(),
    useLlm: $('#useLlm').checked,
    provider: providerRequest()
  };
}

function renderSteps() {
  const completed = state.files.length > 0 ? 7 : state.plan ? 4 : 0;
  $('#flowSummary').textContent = `${completed} / ${steps.length}`;
  $('#steps').innerHTML = steps.map((step, index) => {
    const done = index < completed;
    const active = index === completed || (completed === steps.length && index === steps.length - 1);
    const stateText = done ? 'done' : active ? 'now' : 'todo';
    return `
      <div class="step ${done ? 'done' : ''} ${active ? 'active' : ''}">
        <strong>${index + 1}</strong>
        <span>${step}</span>
        <em class="step-state">${stateText}</em>
      </div>
    `;
  }).join('');
}

function renderProviders() {
  const providers = state.health?.providers || [];
  $('#providerName').innerHTML = providers.map((provider) => `
    <option value="${provider.name}">${provider.name}</option>
  `).join('');
  const preferred = providers.find((provider) => provider.name === 'deepseek')
    || providers.find((provider) => provider.name === 'openai')
    || providers[0];
  if (preferred) {
    $('#providerName').value = preferred.name;
    $('#modelName').value = preferred.name === 'deepseek' ? 'deepseek-reasoner' : (preferred.model || '');
    $('#baseUrl').value = preferred.baseUrl || '';
    updateModelChip();
  }
}

function renderPromptOptions() {
  const select = $('#promptPath');
  if (!select) return;

  if (state.prompts.length === 0) {
    select.innerHTML = '<option value="">prompt/ 中暂无需求文档</option>';
    $('#loadPromptBtn').disabled = true;
    $('#promptStatus').textContent = 'prompt0 默认生效';
    return;
  }

  select.innerHTML = [
    '<option value="">手动输入</option>',
    ...state.prompts.map((prompt) => {
      const label = prompt.directory ? `${prompt.directory}/${prompt.name}` : prompt.name;
      return `<option value="${escapeAttr(prompt.path)}">${escapeHtml(label)}</option>`;
    })
  ].join('');
  $('#loadPromptBtn').disabled = false;
  $('#promptStatus').textContent = `${state.prompts.length} docs, prompt0 默认生效`;
}

function updateModelChip() {
  const model = $('#modelName')?.value || '';
  const provider = $('#providerName')?.value || '';
  const label = provider ? `${provider}${model ? ` / ${model}` : ''}` : '未连接';
  $('#modelChip').textContent = `Model: ${label}`;
  $('#providerStatus').textContent = $('#useLlm')?.checked ? 'LLM' : 'Fallback';
}

function renderSdk() {
  if (!state.sdk) {
    $('#sdkGrid').innerHTML = '<div class="sdk-card"><h4>SDK</h4><p>尚未扫描。</p></div>';
    return;
  }

  const components = state.sdk.components || [];
  const boards = state.sdk.boards || [];
  $('#sdkGrid').innerHTML = `
    <div class="sdk-card">
      <h4>Components</h4>
      <p>${components.length} 个组件</p>
      <div class="sdk-list">${components.map((item) => `<span class="pill">${item.name}</span>`).join('')}</div>
    </div>
    <div class="sdk-card">
      <h4>BSP</h4>
      <p>${escapeHtml((state.sdk.bsp || []).join(', ') || '无')}</p>
    </div>
    <div class="sdk-card">
      <h4>Boards</h4>
      <div class="sdk-list">${boards.map((item) => `<span class="pill">${item.name}</span>`).join('')}</div>
    </div>
    <div class="sdk-card">
      <h4>Examples</h4>
      <div class="sdk-list">${(state.sdk.examples || []).map((name) => `<span class="pill">${name}</span>`).join('')}</div>
    </div>
  `;
}

function extractMermaid(markdown) {
  const match = String(markdown || '').match(/```mermaid\s*([\s\S]*?)```/);
  return match ? match[1].trim() : '';
}

function parsePlanSections(markdown) {
  const text = String(markdown || '').replace(/\r\n/g, '\n');
  if (!text.trim()) return [];
  const headingPattern = /^(#{2,4})\s+(.+)$/gm;
  const matches = [...text.matchAll(headingPattern)];
  if (matches.length === 0) {
    return [{ title: '规划全文', body: text.trim() }];
  }
  return matches.slice(0, 12).map((match, index) => {
    const start = match.index + match[0].length;
    const end = matches[index + 1]?.index ?? text.length;
    return {
      title: match[2].replace(/\*/g, '').trim(),
      body: text.slice(start, end).trim()
    };
  }).filter((section) => section.body);
}

function renderPlan() {
  const planText = state.plan || '';
  $('#planOutput').className = planText ? 'markdown-body' : 'markdown-body empty';
  $('#planOutput').innerHTML = planText ? markdownToHtml(planText) : '等待生成规划。';
  $('#planMeta').textContent = planText ? `${planText.length} chars` : 'Markdown';
  $('#planSummary').textContent = planText ? '已生成规划，可查看结构化拆分、框图和资源表。' : '尚未生成规划。';

  const sections = parsePlanSections(planText);
  if (sections.length === 0) {
    $('#planCards').className = 'plan-cards empty';
    $('#planCards').textContent = '等待生成规划。';
  } else {
    $('#planCards').className = 'plan-cards';
    $('#planCards').innerHTML = sections.slice(0, 6).map((section) => `
      <article class="plan-card">
        <h4>${escapeHtml(section.title)}</h4>
        <div class="markdown-body compact">${markdownToHtml(section.body.slice(0, 1400))}</div>
      </article>
    `).join('');
  }

  renderHardwareCoverage();
  renderSteps();
}

function classifyNode(label) {
  if (/driver|service|SDK|BSP|应用|Application|Layer/i.test(label)) return 'sdk';
  if (/GPIO|I2C|UART|PWM|ESP32|OLED|VL53|ToF|Servo|Button|LED|WiFi|Motor|舵机|按键|传感器|主控/i.test(label)) return 'hardware';
  return '';
}

function parseMermaidNodes(source) {
  const nodes = new Map();
  const nodePattern = /([A-Za-z0-9_]+)\[([^\]]+)\]/g;
  let match = nodePattern.exec(source);
  while (match) {
    nodes.set(match[1], match[2]);
    match = nodePattern.exec(source);
  }
  return [...nodes.entries()].map(([id, label]) => ({ id, label, type: classifyNode(label) }));
}

function renderDiagram() {
  const diagram = extractMermaid(state.plan);
  $('#diagramOutput').textContent = diagram || '规划中暂无 Mermaid 框图。';

  const nodes = parseMermaidNodes(diagram);
  const html = nodes.length > 0
    ? `
      <div class="diagram-layer">
        <h4>硬件 / SDK 节点</h4>
        <div class="diagram-nodes">
          ${nodes.map((node) => `<span class="diagram-node ${node.type}">${escapeHtml(node.label)}</span>`).join('')}
        </div>
      </div>
      <div class="diagram-layer">
        <h4>连接关系</h4>
        <pre>${escapeHtml(diagram.split('\n').filter((line) => /-->|---|<-->|==>/.test(line)).slice(0, 16).join('\n') || '未提取到连接关系。')}</pre>
      </div>
    `
    : '等待生成框图。';

  for (const selector of ['#diagramGrid', '#diagramPreview']) {
    const el = $(selector);
    el.className = nodes.length > 0 ? 'diagram-canvas' : 'diagram-canvas empty';
    el.innerHTML = html;
  }
}

function parseMarkdownTables(markdown) {
  const lines = String(markdown || '').split(/\r?\n/);
  const tables = [];
  for (let index = 0; index < lines.length - 1; index += 1) {
    if (!lines[index].includes('|') || !/^\s*\|?\s*:?-{3,}/.test(lines[index + 1])) continue;
    const tableLines = [lines[index], lines[index + 1]];
    let cursor = index + 2;
    while (cursor < lines.length && lines[cursor].includes('|')) {
      tableLines.push(lines[cursor]);
      cursor += 1;
    }
    tables.push(tableLines);
    index = cursor;
  }
  return tables;
}

function tableToHtml(tableLines) {
  const rows = tableLines
    .filter((line, index) => index !== 1)
    .map((line) => line.trim().replace(/^\|/, '').replace(/\|$/, '').split('|').map((cell) => cell.trim()));
  if (rows.length === 0) return '';
  const head = rows[0];
  const body = rows.slice(1);
  return `
    <table>
      <thead><tr>${head.map((cell) => `<th>${escapeHtml(cell)}</th>`).join('')}</tr></thead>
      <tbody>${body.map((row) => `<tr>${row.map((cell) => `<td>${escapeHtml(cell)}</td>`).join('')}</tr>`).join('')}</tbody>
    </table>
  `;
}

function boardResourceRows() {
  const boardName = $('#boardName')?.value || 'my_board_esp32s3';
  const board = (state.sdk?.boards || []).find((item) => item.name === boardName)
    || (state.sdk?.boards || []).find((item) => item.name.includes('esp32s3'));
  const defines = board?.defines || {};
  const rows = [
    ['LED', 'GPIO/PWM', defines.BOARD_LED_GPIO || '-', 'driver_led', 'board'],
    ['Button', 'GPIO', defines.BOARD_BUTTON_GPIO || '-', 'driver_button', 'board'],
    ['Servo 0', 'PWM', defines.BOARD_SERVO_GPIO_0 || '-', 'driver_servo', 'board'],
    ['Servo 1', 'PWM', defines.BOARD_SERVO_GPIO_1 || '-', 'driver_servo', 'board'],
    ['I2C', 'I2C', `SDA ${defines.BOARD_I2C_SDA_GPIO || '-'} / SCL ${defines.BOARD_I2C_SCL_GPIO || '-'}`, 'bsp_i2c', 'board'],
    ['UART', 'UART', `TX ${defines.BOARD_UART_TX_GPIO || '-'} / RX ${defines.BOARD_UART_RX_GPIO || '-'}`, 'bsp_uart', 'board']
  ];
  return `
    <table>
      <thead><tr><th>资源</th><th>接口</th><th>引脚</th><th>SDK</th><th>来源</th></tr></thead>
      <tbody>${rows.map((row) => `<tr>${row.map((cell) => `<td>${escapeHtml(cell)}</td>`).join('')}</tr>`).join('')}</tbody>
    </table>
  `;
}

function renderResources() {
  const tables = parseMarkdownTables(state.plan);
  const resourceTable = tables.find((table) => /GPIO|I2C|PWM|UART|接口|引脚|资源|ESP32|Servo|OLED|VL53/i.test(table.join('\n')));
  if (resourceTable) {
    $('#resourceMeta').textContent = '来自 LLM 规划';
    $('#resourceTable').className = 'resource-table';
    $('#resourceTable').innerHTML = tableToHtml(resourceTable);
  } else if (state.sdk) {
    $('#resourceMeta').textContent = '来自 boards/ 定义';
    $('#resourceTable').className = 'resource-table';
    $('#resourceTable').innerHTML = boardResourceRows();
  } else {
    $('#resourceTable').className = 'resource-table empty';
    $('#resourceTable').textContent = '等待生成资源表。';
  }
}

function renderHardwareCoverage() {
  const text = `${$('#requirement')?.value || ''}\n${state.plan || ''}`;
  const hits = hardwareKeywords.map(([name, pattern]) => ({ name, hit: pattern.test(text) }));
  $('#hardwareCoverage').innerHTML = hits.map((item) => `
    <span class="pill ${item.hit ? '' : 'warn'}">${item.hit ? 'OK' : 'MISS'} ${item.name}</span>
  `).join('');
}

function renderValidation() {
  const validation = state.validation;
  if (!validation) {
    $('#validationBadge').textContent = '未生成';
    $('#validationPanel').className = 'validation-list empty';
    $('#validationPanel').textContent = '等待代码生成。';
    return;
  }

  $('#validationBadge').textContent = validation.ok ? '通过' : '需处理';
  const items = [];
  if (validation.ok) items.push({ kind: 'ok', text: '静态校验通过' });
  for (const error of validation.errors || []) items.push({ kind: 'error', text: error });
  for (const warning of validation.warnings || []) items.push({ kind: 'warn', text: warning });
  $('#validationPanel').className = 'validation-list';
  $('#validationPanel').innerHTML = items.map((item) => `
    <div class="validation-item ${item.kind}">${escapeHtml(item.text)}</div>
  `).join('');
}

function renderFiles() {
  const projectFiles = state.files.filter((file) => file.path.startsWith('project/'));
  if (projectFiles.length === 0) {
    $('#fileTree').className = 'file-tree empty';
    $('#fileTree').textContent = '等待生成文件。';
    $('#selectedFileTitle').textContent = '未选择文件';
    $('#selectedFileContent').textContent = '生成代码后，在左侧选择文件查看。';
    $('#codeStatus').textContent = '尚未生成代码。';
    $('#codeSummary').textContent = '尚未生成代码。';
    $('#writeBtn').disabled = true;
    $('#buildBtn').disabled = true;
    $('#flashBtn').disabled = true;
    $('#monitorBtn').disabled = true;
    renderValidation();
    renderSteps();
    return;
  }

  if (!state.selectedFilePath || !projectFiles.some((file) => file.path === state.selectedFilePath)) {
    state.selectedFilePath = projectFiles[0].path;
  }

  $('#codeStatus').textContent = `${projectFiles.length} 个 project/ 文件`;
  $('#codeSummary').textContent = state.validation?.ok === false
    ? '代码已生成，但静态校验发现问题。'
    : `已生成 ${projectFiles.length} 个可写入文件。`;
  $('#writeBtn').disabled = false;
  $('#buildBtn').disabled = false;
  $('#flashBtn').disabled = false;
  $('#monitorBtn').disabled = false;

  $('#fileTree').className = 'file-tree';
  $('#fileTree').innerHTML = projectFiles.map((file) => `
    <button class="file-item ${file.path === state.selectedFilePath ? 'active' : ''}" data-file="${escapeHtml(file.path)}">
      ${escapeHtml(file.path)}
    </button>
  `).join('');

  const selected = projectFiles.find((file) => file.path === state.selectedFilePath);
  $('#selectedFileTitle').textContent = selected?.path || '未选择文件';
  $('#selectedFileContent').textContent = selected?.content || '';
  renderValidation();
  renderSteps();
}

function switchTab(name) {
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.classList.toggle('active', tab.dataset.tab === name);
  });
  document.querySelectorAll('.view').forEach((panel) => {
    panel.classList.toggle('active', panel.id === `tab-${name}`);
  });
}

async function loadHealth() {
  state.health = await api('/api/health');
  renderProviders();
  const execution = state.health.execution || {};
  if (execution.defaultCwd) {
    $('#execCwd').value = execution.defaultCwd;
  }
  $('#execBtn').disabled = execution.enabled === false;
  $('#physicalOutput').textContent = JSON.stringify(state.health.physical, null, 2);
  setStatus(`本地 agent 已连接：${state.health.sdkRoot}`, 'ok');
  logActivity('Agent connected', 'ok');
}

async function scanSdk(refresh = false) {
  setStatus('正在扫描 SDK...', 'warn');
  const data = refresh
    ? await api('/api/sdk/refresh', { method: 'POST', body: {} })
    : await api('/api/sdk/summary');
  state.sdk = data.summary;
  renderSdk();
  renderResources();
  setStatus(`SDK 已扫描：${state.sdk.components.length} components, ${state.sdk.examples.length} examples`, 'ok');
  logActivity(`SDK scanned: ${state.sdk.components.length} components`, 'ok');
}

async function loadPromptDocuments() {
  const data = await api('/api/prompts');
  state.prompts = data.prompts || [];
  renderPromptOptions();
  logActivity(`Prompt docs scanned: ${state.prompts.length}`, state.prompts.length ? 'ok' : 'warn');
}

async function loadSelectedPrompt() {
  const promptPath = $('#promptPath').value;
  if (!promptPath) return;
  $('#promptStatus').textContent = '读取中...';
  const data = await api('/api/prompts/read', {
    method: 'POST',
    body: { promptPath }
  });
  $('#requirement').value = data.prompt.content || '';
  $('#promptStatus').textContent = data.prompt.path;
  renderHardwareCoverage();
  logActivity(`Loaded prompt/${data.prompt.path}`, 'ok');
}

async function generatePlan() {
  const payload = currentPayload();
  $('#planOutput').className = 'markdown-body empty';
  $('#planOutput').textContent = '正在生成规划...';
  switchTab('overview');
  logActivity('Generating planning artifacts...', 'warn');
  const data = await api('/api/agent/plan', {
    method: 'POST',
    body: payload
  });
  state.plan = data.warning ? `${data.result}\n\n[Fallback reason] ${data.warning}` : data.result;
  if (data.sdkSummary) {
    state.sdk = data.sdkSummary;
    renderSdk();
  }
  renderPlan();
  renderDiagram();
  renderResources();
  logActivity(data.mode === 'llm' ? 'LLM planning complete' : 'Fallback planning complete', data.warning ? 'warn' : 'ok');
}

async function generateCode() {
  const payload = currentPayload();
  $('#codeStatus').textContent = '正在生成固件草稿...';
  switchTab('code');
  logActivity('Generating firmware files...', 'warn');
  const data = await api('/api/agent/codegen', {
    method: 'POST',
    body: {
      ...payload,
      plan: state.plan
    }
  });
  state.validation = data.validation || null;
  state.files = data.files || [];
  state.selectedFilePath = '';
  renderFiles();
  logActivity(data.note || 'Code generation complete', state.validation?.ok === false ? 'warn' : 'ok');
}

async function writeProject() {
  const files = state.files.filter((file) => file.path.startsWith('project/'));
  const data = await api('/api/project/write', {
    method: 'POST',
    body: { files }
  });
  $('#codeStatus').textContent = `已写入 ${data.written.length} 个文件`;
  logActivity(`Wrote ${data.written.length} files to project/`, 'ok');
}

function renderToolResult(data) {
  $('#toolOutput').textContent = [
    data.cwd ? `cwd: ${data.cwd}` : '',
    `$ ${data.command}`,
    '',
    data.result.stdout || '',
    data.result.stderr || '',
    data.result.error ? `ERROR: ${data.result.error}` : '',
    data.result.timedOut ? 'timed out: true' : '',
    data.result.truncated ? 'output truncated: true' : '',
    `exit code: ${data.result.code}`
  ].filter(Boolean).join('\n');
}

async function runTool(kind) {
  const payload = currentPayload();
  const projectPath = `project/${payload.projectName || 'robot_agent_app'}`;
  $('#toolOutput').textContent = `正在运行 ${kind}...`;
  switchTab('sdk');
  logActivity(`Running ${kind}...`, 'warn');
  const data = await api(`/api/tools/${kind}`, {
    method: 'POST',
    body: {
      projectPath,
      target: payload.target,
      board: payload.board,
      port: payload.port,
      timeoutMs: kind === 'monitor' ? 15000 : 0
    }
  });
  renderToolResult(data);
  logActivity(`${kind} finished with code ${data.result.code}`, data.result.ok ? 'ok' : 'error');
}

async function runExec() {
  const command = $('#execCommand').value.trim();
  if (!command) return;
  $('#toolOutput').textContent = `Running ${command}...`;
  switchTab('sdk');
  logActivity(`Running exec: ${command}`, 'warn');
  const data = await api('/api/tools/exec', {
    method: 'POST',
    body: {
      command,
      argsText: $('#execArgs').value,
      cwd: $('#execCwd').value,
      timeoutMs: 30000
    }
  });
  renderToolResult(data);
  logActivity(`exec finished with code ${data.result.code}`, data.result.ok ? 'ok' : 'error');
}

async function askQuestion() {
  const question = $('#question').value.trim();
  if (!question) return;
  const log = $('#chatLog');
  log.insertAdjacentHTML('beforeend', `<div class="message user"><strong>你</strong>${escapeHtml(question)}</div>`);
  $('#question').value = '';
  const payload = currentPayload();
  logActivity('Q&A request sent', 'warn');
  const data = await api('/api/agent/chat', {
    method: 'POST',
    body: {
      ...payload,
      question,
      currentPlan: state.plan
    }
  });
  log.insertAdjacentHTML('beforeend', `<div class="message"><strong>Agent</strong><div class="markdown-body compact">${markdownToHtml(data.answer)}</div></div>`);
  log.scrollTop = log.scrollHeight;
  logActivity('Q&A answered', data.warning ? 'warn' : 'ok');
}

async function saveSession() {
  const title = $('#projectName').value.trim() || 'Moce hardware session';
  const data = await api('/api/sessions', {
    method: 'POST',
    body: {
      id: state.currentSessionId,
      title,
      requirement: $('#requirement').value,
      plan: state.plan,
      files: state.files
    }
  });
  state.currentSessionId = data.session.id;
  setStatus(`会话已保存：${data.session.title}`, 'ok');
  logActivity(`Session saved: ${data.session.title}`, 'ok');
}

function bindEvents() {
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.addEventListener('click', () => switchTab(tab.dataset.tab));
  });
  $('#providerName').addEventListener('change', () => {
    const provider = state.health.providers.find((item) => item.name === $('#providerName').value);
    $('#modelName').value = provider?.name === 'deepseek' ? 'deepseek-reasoner' : (provider?.model || '');
    $('#baseUrl').value = provider?.baseUrl || '';
    updateModelChip();
  });
  $('#modelName').addEventListener('input', updateModelChip);
  $('#useLlm').addEventListener('change', updateModelChip);
  $('#requirement').addEventListener('input', renderHardwareCoverage);
  $('#loadPromptBtn').addEventListener('click', () => loadSelectedPrompt().catch(showError));
  $('#promptPath').addEventListener('change', () => {
    const selected = $('#promptPath').value;
    $('#promptStatus').textContent = selected || 'manual';
  });
  $('#fileTree').addEventListener('click', (event) => {
    const button = event.target.closest('.file-item');
    if (!button) return;
    state.selectedFilePath = button.dataset.file;
    renderFiles();
  });
  $('#scanBtn').addEventListener('click', () => scanSdk(true).catch(showError));
  $('#planBtn').addEventListener('click', () => generatePlan().catch(showError));
  $('#codeBtn').addEventListener('click', () => generateCode().catch(showError));
  $('#writeBtn').addEventListener('click', () => writeProject().catch(showError));
  $('#buildBtn').addEventListener('click', () => runTool('build').catch(showError));
  $('#flashBtn').addEventListener('click', () => runTool('flash').catch(showError));
  $('#monitorBtn').addEventListener('click', () => runTool('monitor').catch(showError));
  $('#execBtn').addEventListener('click', () => runExec().catch(showError));
  $('#askBtn').addEventListener('click', () => askQuestion().catch(showError));
  $('#question').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') askQuestion().catch(showError);
  });
  $('#saveBtn').addEventListener('click', () => saveSession().catch(showError));
}

function showError(error) {
  setStatus(error.message, 'error');
  logActivity(error.message, 'error');
  console.error(error);
}

async function boot() {
  renderSteps();
  renderSdk();
  renderPlan();
  renderDiagram();
  renderResources();
  renderFiles();
  bindEvents();
  await loadHealth();
  await loadPromptDocuments();
  await scanSdk();
}

boot().catch(showError);
