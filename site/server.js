// GILB's download site: the static page in public/, /api/latest, the newest GitHub release of the
// firmware (its version, date, notes and image files), and /api/betas, the newest pre-releases (beta
// builds, for testing). No dependencies: Node 18+ (fetch). Railway (or any host) gives the port in PORT.
//
// GitHub's API allows 60 requests an hour per IP address without a token, and a host's outgoing IP is
// shared with other apps: on Railway it answered 403 before the site asked once (2026-11-26). So:
//   - one request (the list of releases) serves both endpoints
//   - it is asked again only after CACHE_MINUTES, with the last answer's ETag: an unchanged list comes
//     back as 304, which GitHub does not count against the limit
//   - a refusal keeps the last good answer, and the next try waits RETRY_MINUTES (not every visitor)
//   - GITHUB_TOKEN (any token, no permissions needed for a public repo) raises the limit to 5000 an hour
//
// Env: PORT (default 3000), REPO (default AlanRSV/GILB), GITHUB_TOKEN (optional, recommended),
// CACHE_MINUTES (default 10), RETRY_MINUTES (default 2), BETAS (how many pre-releases, default 3),
// MOCK_RELEASE / MOCK_BETAS (testing: the answers from files instead of GitHub).
'use strict';

const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const PORT = Number(process.env.PORT) || 3000;
const REPO = process.env.REPO || 'AlanRSV/GILB';
const CACHE_MS = (Number(process.env.CACHE_MINUTES) || 10) * 60 * 1000;
const RETRY_MS = (Number(process.env.RETRY_MINUTES) || 2) * 60 * 1000;
const BETAS = Number(process.env.BETAS) || 3;
const PUBLIC = path.join(__dirname, 'public');
const TYPES = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.png': 'image/png', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.json': 'application/json',
};

// The update files GILB builds, by board (README: Install).
const BOARDS = [
  { file: 'HIFIEC90.IMG', board: '8 GB' },
  { file: 'MINIV900.IMG', board: '512 MB' },
];

// A GitHub release as the page shows it.
function shape(j) {
  const files = (j.assets || []).map((a) => {
    const b = BOARDS.find((x) => x.file.toLowerCase() === a.name.toLowerCase());
    return {
      name: a.name, board: b ? b.board : null, size: a.size, url: a.browser_download_url, downloads: a.download_count,
      sha256: a.digest && a.digest.startsWith('sha256:') ? a.digest.slice(7) : null,
    };
  });
  return { tag: j.tag_name, name: j.name || j.tag_name, date: j.published_at, url: j.html_url, notes: j.body_html || '', files };
}

// The list of releases, newest first: { list, etag, at, error, tryAt }.
let gh = { list: null, etag: null, at: 0, error: null, tryAt: 0 };
let pending = null;                          // (one request at a time, however many visitors wait)

async function refresh() {
  const headers = { Accept: 'application/vnd.github.html+json', 'User-Agent': 'gilb-download-site' };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  if (gh.etag && gh.list) headers['If-None-Match'] = gh.etag;
  try {
    const r = await fetch(`https://api.github.com/repos/${REPO}/releases?per_page=30`, { headers });
    if (r.status === 304) {
      gh = { ...gh, at: Date.now(), error: null, tryAt: 0 };
      return;
    }
    if (!r.ok) {
      const left = r.headers.get('x-ratelimit-remaining');
      throw new Error(`GitHub answered ${r.status}${left === '0' ? ' (its hourly limit for this server is used up)' : ''}`);
    }
    const list = (await r.json()).filter((j) => !j.draft)
      .sort((a, b) => String(b.published_at).localeCompare(String(a.published_at)));
    gh = { list, etag: r.headers.get('etag'), at: Date.now(), error: null, tryAt: 0 };
  } catch (e) {
    gh = { ...gh, error: String(e.message || e), tryAt: Date.now() + RETRY_MS };
    console.log(`releases: ${gh.error}${gh.list ? ' (keeping the last answer)' : ''}`);
  }
}

async function releases() {
  const now = Date.now();
  const stale = !gh.list || now - gh.at >= CACHE_MS;
  if (stale && now >= gh.tryAt) {
    if (!pending) pending = refresh().finally(() => { pending = null; });
    await pending;
  }
  return gh;
}

// { status, body } for /api/latest and /api/betas.
async function answer(which) {
  const mock = which === 'latest' ? process.env.MOCK_RELEASE : process.env.MOCK_BETAS;
  if (mock) return { status: 200, body: fs.readFileSync(mock, 'utf8') };
  const g = await releases();
  if (!g.list)
    return { status: 502, body: JSON.stringify({ repo: REPO, error: g.error || 'no answer from GitHub yet' }) };
  if (which === 'latest') {
    const rel = g.list.find((j) => !j.prerelease);
    return { status: 200, body: JSON.stringify({ repo: REPO, release: rel ? shape(rel) : null }) };
  }
  const betas = g.list.filter((j) => j.prerelease).slice(0, BETAS).map(shape);
  return { status: 200, body: JSON.stringify({ repo: REPO, betas }) };
}

function send(res, status, type, body, extra = {}) {
  res.writeHead(status, { 'Content-Type': type, 'X-Content-Type-Options': 'nosniff', ...extra });
  res.end(body);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/api/latest' || url.pathname === '/api/betas') {
    const c = await answer(url.pathname.slice(5));
    return send(res, c.status, TYPES['.json'], c.body, { 'Cache-Control': c.status === 200 ? 'public, max-age=60' : 'no-store' });
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

server.listen(PORT, () => console.log(`GILB site on port ${PORT} (releases of ${REPO}${process.env.GITHUB_TOKEN ? ', with a token' : ', no token: 60 API requests an hour, shared with this host'})`));
