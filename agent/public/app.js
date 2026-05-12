const state = {
  health: null,
  sdk: null,
  plan: '',
  files: [],
  currentSessionId: null
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

const $ = (selector) => document.querySelector(selector);

function setStatus(text, kind = '') {
  const el = $('#healthLine');
  el.className = kind ? `status-${kind}` : '';
  el.textContent = text;
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

function renderSteps() {
  $('#steps').innerHTML = steps.map((step, index) => `
    <div class="step ${index < 4 ? 'active' : ''}">
      <strong>${index + 1}</strong>
      <span>${step}</span>
    </div>
  `).join('');
}

function renderProviders() {
  const providers = state.health?.providers || [];
  $('#providerName').innerHTML = providers.map((provider) => `
    <option value="${provider.name}">${provider.name}</option>
  `).join('');
  const preferred = providers.find((provider) => provider.name === 'openai') || providers[0];
  if (preferred) {
    $('#providerName').value = preferred.name;
    $('#modelName').value = preferred.model || '';
    $('#baseUrl').value = preferred.baseUrl || '';
  }
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
      <p>${(state.sdk.bsp || []).join(', ') || '无'}</p>
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

function renderDiagram() {
  const diagram = extractMermaid(state.plan);
  $('#diagramOutput').textContent = diagram || '规划中暂无 Mermaid 框图。';
  $('#diagramGrid').innerHTML = `
    <div class="node"><h4>需求</h4><p>产品目标、用户场景、功能和约束。</p></div>
    <div class="node"><h4>规划</h4><p>任务拆解、器件选型、资源分配。</p></div>
    <div class="node"><h4>实现</h4><p>只在 project/ 下生成 ESP-IDF 应用工程。</p></div>
    <div class="node"><h4>调试</h4><p>构建、烧录、日志、physical agent 占位接口。</p></div>
  `;
}

function renderFiles() {
  const container = $('#filesOutput');
  if (state.files.length === 0) {
    container.innerHTML = '';
    $('#codeStatus').textContent = '尚未生成代码。';
    $('#writeBtn').disabled = true;
    $('#buildBtn').disabled = true;
    $('#flashBtn').disabled = true;
    $('#monitorBtn').disabled = true;
    return;
  }

  $('#codeStatus').textContent = `已生成 ${state.files.length} 个文件草稿。`;
  $('#writeBtn').disabled = false;
  $('#buildBtn').disabled = false;
  $('#flashBtn').disabled = false;
  $('#monitorBtn').disabled = false;
  container.innerHTML = state.files.map((file) => `
    <article class="file-card">
      <h4>${escapeHtml(file.path)}</h4>
      <pre>${escapeHtml(file.content)}</pre>
    </article>
  `).join('');
}

function escapeHtml(value) {
  return String(value || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function switchTab(name) {
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.classList.toggle('active', tab.dataset.tab === name);
  });
  document.querySelectorAll('.panel').forEach((panel) => {
    panel.classList.toggle('active', panel.id === `tab-${name}`);
  });
}

function currentPayload() {
  return {
    requirement: $('#requirement').value.trim(),
    projectName: $('#projectName').value.trim(),
    target: $('#targetName').value.trim(),
    board: $('#boardName').value.trim(),
    port: $('#portName').value.trim(),
    useLlm: $('#useLlm').checked,
    provider: providerRequest()
  };
}

async function loadHealth() {
  state.health = await api('/api/health');
  renderProviders();
  $('#physicalOutput').textContent = JSON.stringify(state.health.physical, null, 2);
  setStatus(`本地 agent 已连接：${state.health.sdkRoot}`, 'ok');
}

async function scanSdk(refresh = false) {
  setStatus('正在扫描 SDK...', 'warn');
  const data = refresh
    ? await api('/api/sdk/refresh', { method: 'POST', body: {} })
    : await api('/api/sdk/summary');
  state.sdk = data.summary;
  renderSdk();
  setStatus(`SDK 已扫描：${state.sdk.components.length} components, ${state.sdk.examples.length} examples`, 'ok');
}

async function generatePlan() {
  const payload = currentPayload();
  $('#planOutput').textContent = '正在生成规划...';
  switchTab('plan');
  const data = await api('/api/agent/plan', {
    method: 'POST',
    body: payload
  });
  state.plan = data.result;
  if (data.sdkSummary) {
    state.sdk = data.sdkSummary;
    renderSdk();
  }
  $('#planOutput').textContent = data.warning ? `${data.result}\n\n[Fallback reason] ${data.warning}` : data.result;
  renderDiagram();
}

async function generateCode() {
  const payload = currentPayload();
  $('#codeStatus').textContent = '正在生成固件草稿...';
  switchTab('code');
  const data = await api('/api/agent/codegen', {
    method: 'POST',
    body: {
      ...payload,
      plan: state.plan
    }
  });
  state.files = data.files || [];
  if (data.result) {
    state.files.unshift({
      path: 'LLM_OUTPUT.md',
      content: data.warning ? `${data.result}\n\n[Fallback reason] ${data.warning}` : data.result
    });
  }
  renderFiles();
}

async function writeProject() {
  const files = state.files.filter((file) => file.path.startsWith('project/'));
  const data = await api('/api/project/write', {
    method: 'POST',
    body: { files }
  });
  $('#codeStatus').textContent = `已写入：${data.written.join(', ')}`;
}

async function runTool(kind) {
  const payload = currentPayload();
  const projectPath = `project/${payload.projectName || 'robot_agent_app'}`;
  $('#toolOutput').textContent = `正在运行 ${kind}...`;
  switchTab('tools');
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
  $('#toolOutput').textContent = [
    `$ ${data.command}`,
    '',
    data.result.stdout || '',
    data.result.stderr || '',
    data.result.error ? `ERROR: ${data.result.error}` : '',
    `exit code: ${data.result.code}`
  ].filter(Boolean).join('\n');
}

async function askQuestion() {
  const question = $('#question').value.trim();
  if (!question) return;
  const log = $('#chatLog');
  log.insertAdjacentHTML('beforeend', `<div class="message user"><strong>你</strong>${escapeHtml(question)}</div>`);
  $('#question').value = '';
  const payload = currentPayload();
  const data = await api('/api/agent/chat', {
    method: 'POST',
    body: {
      ...payload,
      question,
      currentPlan: state.plan
    }
  });
  log.insertAdjacentHTML('beforeend', `<div class="message"><strong>Agent</strong><pre>${escapeHtml(data.answer)}</pre></div>`);
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
}

function bindEvents() {
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.addEventListener('click', () => switchTab(tab.dataset.tab));
  });
  $('#providerName').addEventListener('change', () => {
    const provider = state.health.providers.find((item) => item.name === $('#providerName').value);
    $('#modelName').value = provider?.model || '';
    $('#baseUrl').value = provider?.baseUrl || '';
  });
  $('#scanBtn').addEventListener('click', () => scanSdk(true).catch(showError));
  $('#planBtn').addEventListener('click', () => generatePlan().catch(showError));
  $('#codeBtn').addEventListener('click', () => generateCode().catch(showError));
  $('#writeBtn').addEventListener('click', () => writeProject().catch(showError));
  $('#buildBtn').addEventListener('click', () => runTool('build').catch(showError));
  $('#flashBtn').addEventListener('click', () => runTool('flash').catch(showError));
  $('#monitorBtn').addEventListener('click', () => runTool('monitor').catch(showError));
  $('#askBtn').addEventListener('click', () => askQuestion().catch(showError));
  $('#question').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') {
      askQuestion().catch(showError);
    }
  });
  $('#saveBtn').addEventListener('click', () => saveSession().catch(showError));
}

function showError(error) {
  setStatus(error.message, 'error');
  console.error(error);
}

async function boot() {
  renderSteps();
  renderSdk();
  bindEvents();
  await loadHealth();
  await scanSdk();
  renderDiagram();
}

boot().catch(showError);
