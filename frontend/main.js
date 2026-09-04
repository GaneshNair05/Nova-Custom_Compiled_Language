const DEFAULT_SOURCE = `# Welcome to Nova.
# Pick an example above, or write your own.

loot x = 10
loot y = 3

announce(x + y)
`;

let editor;
let examples = [];

const runBtn = document.getElementById('run-btn');
const clearBtn = document.getElementById('clear-btn');
const copyBtn = document.getElementById('copy-btn');
const examplePicker = document.getElementById('example-picker');
const demoBanner = document.getElementById('demo-banner');
const runtimeStatus = document.getElementById('runtime-status');
const exitBadge = document.getElementById('exit-badge');
const tabButtons = document.querySelectorAll('.tab-btn');

const panels = {
  output: document.getElementById('tab-output'),
  tokens: document.getElementById('tab-tokens'),
  ir: document.getElementById('tab-ir'),
};

function setActiveTab(name) {
  tabButtons.forEach((btn) => {
    const active = btn.dataset.tab === name;
    btn.classList.toggle('active', active);
    btn.setAttribute('aria-selected', String(active));
  });
  Object.entries(panels).forEach(([key, el]) => {
    el.classList.toggle('active', key === name);
  });
}

tabButtons.forEach((btn) => {
  btn.addEventListener('click', () => setActiveTab(btn.dataset.tab));
});

function renderOutput(result) {
  demoBanner.hidden = result.mode !== 'demo';

  panels.tokens.textContent = result.tokens || '// No token data available.';
  panels.ir.textContent = result.ir || '// No IR generated.';

  panels.output.innerHTML = '';

  if (result.error) {
    const box = document.createElement('div');
    box.className = 'output-error-box';
    box.textContent = result.error;
    panels.output.appendChild(box);
    if (!result.output && !result.stderr) return;
  }

  const stdoutText = result.output || '';
  const stderrText = result.stderr || '';

  if (!stdoutText && !stderrText && !result.error) {
    const span = document.createElement('span');
    span.className = 'placeholder';
    span.textContent = '(program produced no output)';
    panels.output.appendChild(span);
    return;
  }

  if (stdoutText) {
    panels.output.appendChild(document.createTextNode(stdoutText + '\n'));
  }
  if (stderrText) {
    const errLine = document.createElement('div');
    errLine.className = 'output-line-error';
    errLine.textContent = stderrText;
    panels.output.appendChild(errLine);
  }

  if (result.timedOut) {
    const note = document.createElement('div');
    note.className = 'output-line-error';
    note.textContent = `⚡ Execution timed out (>8s). Possible infinite loop.`;
    panels.output.appendChild(note);
  }

  // Update Footer status
  if (result.exitCode !== undefined) {
    exitBadge.classList.remove('hidden');
    exitBadge.textContent = `Exit Code: ${result.exitCode}`;
    exitBadge.classList.toggle('error', result.exitCode !== 0);
  } else {
    exitBadge.classList.add('hidden');
  }
}

async function runCode() {
  const code = editor.getValue();
  if (!code.trim()) return;

  runBtn.disabled = true;
  runBtn.querySelector('.btn-text').textContent = 'Compiling…';
  runtimeStatus.textContent = 'Running program…';
  setActiveTab('output');
  panels.output.innerHTML = '<span class="placeholder">⚡ Compiling and executing in sandbox…</span>';

  const startTime = performance.now();

  try {
    const res = await fetch('/api/run', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code }),
    });
    const result = await res.json();
    const duration = ((performance.now() - startTime) / 1000).toFixed(2);
    runtimeStatus.textContent = `Completed in ${duration}s`;
    renderOutput(result);
  } catch (err) {
    panels.output.innerHTML = '';
    const box = document.createElement('div');
    box.className = 'output-error-box';
    box.textContent = `Backend error: ${err.message}`;
    panels.output.appendChild(box);
    runtimeStatus.textContent = 'Failed';
  } finally {
    runBtn.disabled = false;
    runBtn.querySelector('.btn-text').textContent = 'Run';
  }
}

runBtn.addEventListener('click', runCode);

clearBtn.addEventListener('click', () => {
  if (confirm('Clear editor contents?')) {
    editor.setValue('');
    editor.focus();
  }
});

copyBtn.addEventListener('click', () => {
  const activePanel = document.querySelector('.tab-panel.active');
  if (activePanel) {
    navigator.clipboard.writeText(activePanel.innerText);
    copyBtn.style.color = 'var(--accent)';
    setTimeout(() => { copyBtn.style.color = ''; }, 1000);
  }
});

document.addEventListener('keydown', (e) => {
  const isRunShortcut = (e.metaKey || e.ctrlKey) && e.key === 'Enter';
  if (isRunShortcut) {
    e.preventDefault();
    runCode();
  }
});

async function loadExamples() {
  try {
    const res = await fetch('/api/examples');
    examples = await res.json();
    examples.forEach((ex) => {
      const opt = document.createElement('option');
      opt.value = ex.id;
      opt.textContent = ex.title;
      examplePicker.appendChild(opt);
    });
  } catch (err) {
    console.error('Failed to load examples:', err);
  }
}

examplePicker.addEventListener('change', () => {
  const chosen = examples.find((ex) => ex.id === examplePicker.value);
  if (chosen && editor) {
    editor.setValue(chosen.code);
  }
});

// --- Monaco setup ---
require.config({
  paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.47.0/min/vs' },
});

require(['vs/editor/editor.main'], () => {
  registerNovaLanguage(monaco);

  // Modernized editor theme
  monaco.editor.defineTheme('nova-dark-pro', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword', foreground: 'f5a623', fontStyle: 'bold' },
      { token: 'predefined', foreground: '38ef7d' },
      { token: 'comment', foreground: '5a6480', fontStyle: 'italic' },
      { token: 'string', foreground: '7dd3fc' },
      { token: 'number', foreground: 'f472b6' },
      { token: 'operator', foreground: 'f0f3fa' },
    ],
    colors: {
      'editor.background': '#131622',
      'editor.foreground': '#f0f3fa',
      'editorLineNumber.foreground': '#394057',
      'editorLineNumber.activeForeground': '#f5a623',
      'editor.selectionBackground': '#22283d',
      'editorCursor.foreground': '#f5a623',
      'editor.lineHighlightBackground': '#181c2b',
      'editorIndentGuide.background': '#1f2438',
      'editorIndentGuide.activeBackground': '#3e4768',
    },
  });

  editor = monaco.editor.create(document.getElementById('editor'), {
    value: DEFAULT_SOURCE,
    language: 'nova',
    theme: 'nova-dark-pro',
    fontFamily: "'Fira Code', 'JetBrains Mono', monospace",
    fontLigatures: true,
    fontSize: 14,
    lineHeight: 22,
    minimap: { enabled: false },
    automaticLayout: true,
    scrollBeyondLastLine: false,
    padding: { top: 18, bottom: 18 },
    renderLineHighlight: 'line',
    cursorBlinking: 'smooth',
    smoothScrolling: true,
  });

  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, runCode);

  loadExamples();
});