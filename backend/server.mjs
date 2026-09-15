import http from 'node:http';
import fs from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';
import { cadStatus, buildCad } from './cad.mjs';

// The backend is deliberately independent from the frontend. It owns the C executable,
// configuration, generated reports and the /api contract consumed by Vercel.
const backendDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(backendDir, '..');
const dist = path.join(root, 'dist');
const data = path.join(root, 'data');
const runs = path.join(data, 'runs');
const executable = process.platform === 'win32' ? path.join(root, 'build', 'txsim.exe') : path.join(root, 'build', 'txsim');
const port = Number(process.env.PORT || process.env.TXC_PORT || 5173);
const corsOrigin = process.env.CORS_ORIGIN || '*';

const send = (res, status, body, type = 'application/json; charset=utf-8') => {
  res.writeHead(status, {
    'Content-Type': type,
    'Cache-Control': 'no-store',
    'Access-Control-Allow-Origin': corsOrigin,
    'Access-Control-Allow-Headers': 'Content-Type',
    'Access-Control-Allow-Methods': 'GET,POST,OPTIONS'
  });
  res.end(body);
};
const json = (res, status, value) => send(res, status, JSON.stringify(value));

const readBody = (req) => new Promise((resolve, reject) => {
  let body = '';
  req.on('data', (chunk) => {
    body += chunk;
    if (body.length > 100000) req.destroy();
  });
  req.on('end', () => {
    try { resolve(body ? JSON.parse(body) : {}); }
    catch { reject(new Error('Request body must be valid JSON')); }
  });
  req.on('error', reject);
});

function execute(args) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, args, { cwd: root, windowsHide: true });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => { stdout += chunk; });
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('error', (error) => reject(new Error(`Unable to start C simulator: ${error.message}`)));
    child.on('close', (code) => code === 0
      ? resolve({ stdout, stderr })
      : reject(new Error(stderr.trim() || `C simulator exited with code ${code}`)));
  });
}

function sectionArgument(sections) {
  if (!Array.isArray(sections) || sections.length === 0 || sections.includes('all')) return 'all';
  return sections.join(',');
}

function safeOverrideKey(key) { return /^[A-Z0-9_]+$/.test(key); }

const mime = {'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.json':'application/json; charset=utf-8','.glb':'model/gltf-binary','.step':'application/step','.png':'image/png','.txt':'text/plain; charset=utf-8'};
async function serveFrontend(urlPath, res) {
  const relative = urlPath === '/' ? 'index.html' : urlPath.endsWith('/') ? urlPath.replace(/^\/+/, '')+'index.html' : urlPath.replace(/^\/+/, '');
  const target = path.resolve(dist, relative);
  if (!target.startsWith(path.resolve(dist))) return send(res, 404, 'Not found', 'text/plain; charset=utf-8');
  try { return send(res, 200, await fs.readFile(target), mime[path.extname(target)] || 'application/octet-stream'); }
  catch { return send(res, 404, 'Not found', 'text/plain; charset=utf-8'); }
}

async function simulate(body) {
  if (!existsSync(executable)) {
    throw new Error('C simulator is not built. Build the backend before running a simulation.');
  }
  const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  const runDir = path.join(runs, id);
  await fs.mkdir(runDir, { recursive: true });
  const args = [
    '--mode', ['nominal', 'explore', 'optimize'].includes(body.mode) ? body.mode : 'nominal',
    '--sections', sectionArgument(body.sections),
    '--config', path.join(data, 'config.txt'),
    '--output', path.join(runDir, 'output.txt'),
    '--json', path.join(runDir, 'output.json'),
    '--stdout-json'
  ];
  for (const [key, value] of Object.entries(body.overrides || {})) {
    if (safeOverrideKey(key) && value !== '' && value !== null && value !== undefined) {
      args.push('--set', `${key}=${value}`);
    }
  }
  const processResult = await execute(args);
  let result;
  try { result = JSON.parse(processResult.stdout); }
  catch { throw new Error(`Simulator returned invalid JSON: ${processResult.stdout.slice(0, 180)}`); }
  await Promise.all([
    fs.copyFile(path.join(runDir, 'output.txt'), path.join(data, 'output.txt')),
    fs.copyFile(path.join(runDir, 'output.json'), path.join(data, 'output.json'))
  ]);
  result.meta = { ...result.meta, runId: id };
  return result;
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
    if (req.method === 'OPTIONS') return send(res, 204, '');
    if (req.method === 'GET' && url.pathname === '/api/cad/status') return json(res, 200, cadStatus());
    if (req.method === 'POST' && url.pathname === '/api/cad/build') return json(res, 200, await buildCad());
    if (req.method === 'GET' && url.pathname === '/api/health') {
      return json(res, 200, { ok: true, executable: existsSync(executable), engine: 'tx-c-c11' });
    }
    if (req.method === 'GET' && url.pathname === '/api/output.txt') {
      try { return send(res, 200, await fs.readFile(path.join(data, 'output.txt')), 'text/plain; charset=utf-8'); }
      catch { return send(res, 404, 'No report yet', 'text/plain; charset=utf-8'); }
    }
    const runReport = url.pathname.match(/^\/api\/runs\/([A-Za-z0-9-]+)\/(output\.txt|output\.json)$/);
    if (req.method === 'GET' && runReport) {
      const runId = runReport[1];
      const filename = runReport[2];
      const runRoot = path.resolve(runs, runId);
      const target = path.resolve(runRoot, filename);
      if (!target.startsWith(`${runRoot}${path.sep}`)) return send(res, 404, 'Report not found', 'text/plain; charset=utf-8');
      try { return send(res, 200, await fs.readFile(target), filename.endsWith('.json') ? 'application/json; charset=utf-8' : 'text/plain; charset=utf-8'); }
      catch { return send(res, 404, 'Report not found', 'text/plain; charset=utf-8'); }
    }
    if (req.method === 'GET' && url.pathname === '/api/output.json') {
      try { return send(res, 200, await fs.readFile(path.join(data, 'output.json')), 'application/json; charset=utf-8'); }
      catch { return send(res, 404, 'No report yet', 'text/plain; charset=utf-8'); }
    }
    if (req.method === 'POST' && url.pathname === '/api/simulate') return json(res, 200, await simulate(await readBody(req)));
    if (req.method === 'GET') return serveFrontend(url.pathname, res);
    return json(res, 404, { error: 'Backend route not found' });
  } catch (error) {
    return json(res, 500, { error: error.message });
  }
});

await fs.mkdir(runs, { recursive: true });
server.listen(port, '0.0.0.0', () => console.log(`TX-C API listening on port ${port}`));
