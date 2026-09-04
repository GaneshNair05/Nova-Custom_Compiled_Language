const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { spawn } = require('child_process');
const { getMockResult } = require('./mockExamples');

// --- Configuration ---
// Point this at your compiled mycompiler(.exe). Unset by default: the
// server runs in demo mode (canned output for the bundled examples only)
// until you set it. See ../../README.md.
const COMPILER_PATH = process.env.NOVA_COMPILER_PATH || null;

const TIMEOUT_MS = Number(process.env.NOVA_TIMEOUT_MS) || 8000;
const MAX_OUTPUT_BYTES = Number(process.env.NOVA_MAX_OUTPUT_BYTES) || 200_000;

// --- Security model (read this before deploying anywhere but your own machine) ---
// This runs your actual compiled binary, which JIT-compiles submitted source
// straight to native machine code via LLVM and executes it in-process. That
// is fundamentally different from sandboxing a script in an interpreted
// language (Python, JS, Lua) — there is no bytecode-level sandbox here, no
// capability restriction, nothing stopping compiled Nova code from doing
// anything the OS lets the compiler's own process do. What this file adds
// (a timeout, an output size cap, a temp working directory, a best-effort
// block on graphical programs) is enough to stop accidental infinite loops
// and runaway output from a well-intentioned script — it is NOT a security
// boundary against someone deliberately trying to break out. If you ever
// deploy this somewhere the public can hit it, run the compiler inside a
// locked-down container (no network, a memory/CPU cgroup limit, a
// non-root user, ideally gVisor or Firecracker rather than a bare
// container) instead of as a subprocess on the host directly.

// Best-effort guard: raylib opens a real OS window, which can't work
// against a headless server (no display) — it'll either error out or hang
// waiting on something that will never happen. This is a plain substring
// check, not a parser, so it can be worked around deliberately; it exists
// to catch the common case (someone pastes in snake.nv) with a clear
// message instead of a confusing timeout.
function usesGraphics(code) {
  return /\binit_window\s*\(/.test(code);
}

function parseSections(stdout) {
  // main.cpp prints these three sections in order, separated by fixed
  // banner lines. Splitting on them is simpler and more robust than trying
  // to reconstruct structure from the compiler's raw output some other way.
  const irMarker = '--- Generating LLVM IR ---';
  const execMarker = '--- Executing Program ---';

  const irIdx = stdout.indexOf(irMarker);
  const execIdx = stdout.indexOf(execMarker);

  if (irIdx === -1 || execIdx === -1) {
    // Didn't find the expected banners (e.g. the binary is a different
    // build, or it crashed before printing them) — hand back everything
    // as "output" rather than silently dropping it.
    return { tokens: '', ir: '', output: stdout };
  }

  const tokens = stdout.slice(0, irIdx).trim();
  const ir = stdout.slice(irIdx + irMarker.length, execIdx).trim();
  const output = stdout.slice(execIdx + execMarker.length).trim();
  return { tokens, ir, output };
}

function runCompiledBinary(code) {
  return new Promise((resolve) => {
    const workDir = fs.mkdtempSync(path.join(os.tmpdir(), 'nova-run-'));
    const sourcePath = path.join(workDir, 'program.nv');
    fs.writeFileSync(sourcePath, code, 'utf8');

    const cleanup = () => {
      fs.rm(workDir, { recursive: true, force: true }, () => {});
    };

    let stdout = '';
    let stderr = '';
    let truncated = false;
    let settled = false;

    let child;
    const compilerDir = path.dirname(COMPILER_PATH);

    try {
      child = spawn(COMPILER_PATH, [sourcePath], {
        cwd: workDir,
        stdio: ['ignore', 'pipe', 'pipe'],
        env: {
          ...process.env,
          PATH: `${compilerDir};${process.env.PATH}`,
        },
      });
    } catch (err) {
      cleanup();
      resolve({
        mode: 'compiler',
        error: `Couldn't start the compiler at "${COMPILER_PATH}": ${err.message}`,
      });
      return;
    }

    const timer = setTimeout(() => {
      if (settled) return;
      child.kill('SIGKILL');
    }, TIMEOUT_MS);

    const cap = (buf, chunk) => {
      if (buf.length >= MAX_OUTPUT_BYTES) {
        truncated = true;
        return buf;
      }
      return buf + chunk.toString('utf8');
    };

    child.stdout.on('data', (chunk) => { stdout = cap(stdout, chunk); });
    child.stderr.on('data', (chunk) => { stderr = cap(stderr, chunk); });

    child.on('error', (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      cleanup();
      resolve({
        mode: 'compiler',
        error: `Couldn't run the compiler at "${COMPILER_PATH}": ${err.message}`,
      });
    });

child.on('close', (exitCode, signal) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      cleanup();

      console.log('--- COMPILER DEBUG START ---');
      console.log('Exit Code:', exitCode);
      console.log('Signal:', signal);
      console.log('Raw stdout:', JSON.stringify(stdout));
      console.log('Raw stderr:', JSON.stringify(stderr));
      console.log('--- COMPILER DEBUG END ---');

      const timedOut = signal === 'SIGKILL';
      const sections = parseSections(stdout);
      resolve({
        mode: 'compiler',
        ...sections,
        stderr,
        exitCode,
        timedOut,
        truncated,
      });
    });
  });
}

async function runNova(code) {
  if (usesGraphics(code)) {
    return {
      mode: 'blocked',
      error:
        "This program opens a window (init_window/draw_rect), which needs a real display — " +
        "the web playground only runs text-output programs for now. Try it on your own machine " +
        "with the compiled binary instead.",
    };
  }

  if (!COMPILER_PATH) {
    return getMockResult(code);
  }

  if (!fs.existsSync(COMPILER_PATH)) {
    return {
      mode: 'compiler',
      error: `NOVA_COMPILER_PATH is set to "${COMPILER_PATH}" but nothing exists there. Check the path (see README.md).`,
    };
  }

  return runCompiledBinary(code);
}

module.exports = { runNova, usesGraphics, parseSections };
