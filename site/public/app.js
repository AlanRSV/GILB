// The download card and the release notes, from /api/latest (server.js: the newest GitHub release).
(function () {
  const box = document.getElementById('release');
  const notes = document.getElementById('notes');
  const notesBody = document.getElementById('notes-body');

  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
  const mb = (n) => (n / 1048576).toFixed(1) + ' MB';
  const day = (iso) => new Date(iso).toLocaleDateString(undefined, { year: 'numeric', month: 'long', day: 'numeric' });

  function fileCard(f, beta) {
    return `<div class="file">
      <span class="board">${esc(f.board)} board</span>
      <span class="name">${esc(f.name)}</span>
      ${f.size ? `<span class="meta">${mb(f.size)}${f.downloads ? ` · ${f.downloads.toLocaleString()} downloads` : ''}</span>` : ''}
      <a class="btn${beta ? '' : ' primary'}" href="${esc(f.url)}" download>Download ${beta ? 'beta ' : ''}for ${esc(f.board)}</a>
      ${f.sha256 ? `<span class="sha">SHA-256 ${esc(f.sha256)}<button type="button" data-copy="${esc(f.sha256)}">copy</button></span>` : ''}
    </div>`;
  }

  function show(d) {
    const repoUrl = `https://github.com/${d.repo}`;
    if (d.error) {                               // (GitHub's API refused the server: its direct links still work)
      box.innerHTML = `<div class="files">${fileCard({
          board: '8 GB', name: 'HIFIEC90.IMG', url: `${repoUrl}/releases/latest/download/HIFIEC90.IMG` })}</div>
        <p class="others">The newest release, straight from GitHub (its size, checksum and notes could not be loaded
          just now). The other files: <a href="${repoUrl}/releases/latest" rel="noopener">the release on GitHub</a> ·
          <a href="${repoUrl}/releases" rel="noopener">all releases</a>. The 512 MB board is still being tested:
          see <a href="#beta">Beta builds</a>.</p>`;
      return;
    }
    if (!d.release) {
      box.innerHTML = `<p><b>No release is published yet.</b> GILB builds from source
        (<a href="${repoUrl}#install" rel="noopener">how</a>); the first ready-made images will appear here.</p>`;
      return;
    }
    const r = d.release;
    const boards = r.files.filter((f) => f.board);
    const rest = r.files.filter((f) => !f.board);
    box.innerHTML = `<div class="rel-head"><h3>${esc(r.name)}</h3><span class="tag">${esc(r.tag)}</span>
        <span class="muted">${day(r.date)}</span></div>
      ${boards.length ? `<div class="files">${boards.map((f) => fileCard(f)).join('')}</div>`
        : `<p>This release has no board images attached. <a href="${esc(r.url)}" rel="noopener">See it on GitHub</a>.</p>`}
      ${rest.length ? `<p class="others">Also in this release: ${rest.map((f) => `<a href="${esc(f.url)}">${esc(f.name)}</a>`).join(', ')}</p>` : ''}
      <p class="others"><a href="${esc(r.url)}" rel="noopener">This release on GitHub</a> ·
        <a href="${repoUrl}/releases" rel="noopener">all releases</a></p>`;
    if (r.notes) {
      notesBody.innerHTML = r.notes;            // (GitHub's own rendering of the notes: sanitized there)
      notes.hidden = false;
    }
  }

  box.addEventListener('click', (e) => {
    const b = e.target.closest('[data-copy]');
    if (!b) return;
    navigator.clipboard.writeText(b.dataset.copy).then(() => { b.textContent = 'copied'; setTimeout(() => { b.textContent = 'copy'; }, 1500); });
  });

  fetch('/api/latest').then((r) => r.json()).then(show)
    .catch(() => show({ repo: 'AlanRSV/GILB', error: 'offline' }));

  // Beta builds: the newest pre-releases (/api/betas); a line instead when there are none.
  const betaBox = document.getElementById('betas');

  function betaCard(r) {
    const boards = r.files.filter((f) => f.board);
    const rest = r.files.filter((f) => !f.board);
    return `<div class="card beta">
      <div class="rel-head"><span class="badge">Beta</span><h3>${esc(r.name)}</h3><span class="tag">${esc(r.tag)}</span>
        <span class="muted">${day(r.date)}</span></div>
      ${boards.length ? `<div class="files">${boards.map((f) => fileCard(f, true)).join('')}</div>`
        : `<p>No board images attached. <a href="${esc(r.url)}" rel="noopener">See it on GitHub</a>.</p>`}
      ${rest.length ? `<p class="others">Also: ${rest.map((f) => `<a href="${esc(f.url)}">${esc(f.name)}</a>`).join(', ')}</p>` : ''}
      ${r.notes ? `<details class="beta-notes"><summary>What's in this beta</summary><div class="prose">${r.notes}</div></details>` : ''}
      <p class="others"><a href="${esc(r.url)}" rel="noopener">This beta on GitHub</a></p>
    </div>`;
  }

  betaBox.addEventListener('click', (e) => {
    const b = e.target.closest('[data-copy]');
    if (!b) return;
    navigator.clipboard.writeText(b.dataset.copy).then(() => { b.textContent = 'copied'; setTimeout(() => { b.textContent = 'copy'; }, 1500); });
  });

  const betaList = (repo) => `https://github.com/${repo}/releases`;
  fetch('/api/betas').then((r) => r.json()).then((d) => {
    if (d.error) {
      betaBox.innerHTML = `<p class="muted">The beta list could not be loaded just now. Betas are the releases marked
        "Pre-release" on <a href="${betaList(d.repo)}" rel="noopener">GitHub's releases page</a>.</p>`;
    } else if (!d.betas || !d.betas.length) {
      betaBox.innerHTML = '<p class="muted">No beta right now: everything being tested is in the release above.</p>';
    } else {
      betaBox.innerHTML = d.betas.map(betaCard).join('');   // (notes: GitHub's own sanitized rendering)
    }
  }).catch(() => {
    betaBox.innerHTML = `<p class="muted">The beta list could not be loaded. See
      <a href="${betaList('AlanRSV/GILB')}" rel="noopener">GitHub's releases page</a>.</p>`;
  });
})();
