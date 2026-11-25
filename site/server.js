// GILB's download site: the static page in public/ and /api/latest, the newest GitHub release of the
// firmware (its version, date, notes and image files), cached so visitors never meet GitHub's rate
// limit. No dependencies: Node 18+ (fetch). Railway (or any host) gives the port in PORT.
//
// Env: PORT (default 3000), REPO (default AlanRSV/GILB), GITHUB_TOKEN (optional: a higher API limit),
// CACHE_MINUTES (default 10).
'use strict';

const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const PORT = Number(process.env.PORT) || 3000;
const REPO = process.env.REPO || 'AlanRSV/GILB';
const CACHE_MS = (Number(process.env.CACHE_MINUTES) || 10) * 60 * 1000;
const PUBLIC = path.join(__dirname, 'public');
const TYPES = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.png': 'image/png', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.json': 'application/json',
};

let cache = { at: 0, body: null, status: 0 };

// The update files GILB builds, by board (README: Install).
const BOARDS = [
  { file: 'HIFIEC90.IMG', board: '8 GB' },
  { file: 'MINIV900.IMG', board: '512 MB' },
];

async function latest() {
  if (process.env.MOCK_RELEASE)              // (testing: a release as /api/latest gives it, from a file)
    return { status: 200, body: fs.readFileSync(process.env.MOCK_RELEASE, 'utf8') };
  if (cache.body && Date.now() - cache.at < CACHE_MS) return cache;
  const headers = { Accept: 'application/vnd.github.html+json', 'User-Agent': 'gilb-download-site' };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  try {
    const r = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, { headers });
    if (r.status === 404) {
      cache = { at: Date.now(), status: 200, body: JSON.stringify({ repo: REPO, release: null }) };
      return cache;
    }
    if (!r.ok) throw new Error(`GitHub answered ${r.status}`);
    const j = await r.json();
    const files = j.assets.map((a) => {
      const b = BOARDS.find((x) => x.file.toLowerCase() === a.name.toLowerCase());
      return {
        name: a.name, board: b ? b.board : null, size: a.size, url: a.browser_download_url, downloads: a.download_count,
        sha256: a.digest && a.digest.startsWith('sha256:') ? a.digest.slice(7) : null,
      };
    });
    cache = {
      at: Date.now(), status: 200,
      body: JSON.stringify({
        repo: REPO,
        release: { tag: j.tag_name, name: j.name || j.tag_name, date: j.published_at, url: j.html_url, notes: j.body_html || '', files },
      }),
    };
  } catch (e) {
    if (!cache.body) cache = { at: 0, status: 502, body: JSON.stringify({ repo: REPO, error: String(e.message || e) }) };
  }
  return cache;
}

function send(res, status, type, body, extra = {}) {
  res.writeHead(status, { 'Content-Type': type, 'X-Content-Type-Options': 'nosniff', ...extra });
  res.end(body);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/api/latest') {
    const c = await latest();
    return send(res, c.status, TYPES['.json'], c.body, { 'Cache-Control': 'public, max-age=60' });
  }
  if (url.pathname === '/healthz') return send(res, 200, 'text/plain', 'ok');
  let p = decodeURIComponent(url.pathname);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.normalize(path.join(PUBLIC, p));
  if (!file.startsWith(PUBLIC + path.sep)) return send(res, 403, 'text/plain', 'forbidden');
  fs.readFile(file, (err, data) => {
    if (err) return send(res, 404, 'text/plain', 'not found');
    send(res, 200, TYPES[path.extname(file)] || 'application/octet-stream', data,
      { 'Cache-Control': p.endsWith('.html') ? 'no-cache' : 'public, max-age=3600' });
  });
});

server.listen(PORT, () => console.log(`GILB site on port ${PORT} (releases of ${REPO})`));
