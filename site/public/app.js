// The download card and the release notes, from /api/latest (server.js: the newest GitHub release).
(function () {
  const box = document.getElementById('release');
  const notes = document.getElementById('notes');
  const notesBody = document.getElementById('notes-body');

  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
  const mb = (n) => (n / 1048576).toFixed(1) + ' MB';
  const day = (iso) => new Date(iso).toLocaleDateString(undefined, { year: 'numeric', month: 'long', day: 'numeric' });

  function fileCard(f) {
    return `<div class="file">
      <span class="board">${esc(f.board)} board</span>
      <span class="name">${esc(f.name)}</span>
      <span class="meta">${mb(f.size)}${f.downloads ? ` · ${f.downloads.toLocaleString()} downloads` : ''}</span>
      <a class="btn primary" href="${esc(f.url)}" download>Download for ${esc(f.board)}</a>
      ${f.sha256 ? `<span class="sha">SHA-256 ${esc(f.sha256)}<button type="button" data-copy="${esc(f.sha256)}">copy</button></span>` : ''}
    </div>`;
  }

  function show(d) {
    const repoUrl = `https://github.com/${d.repo}`;
    if (d.error) {
      box.innerHTML = `<p>The release list could not be read just now. The files are on
        <a href="${repoUrl}/releases/latest" rel="noopener">GitHub's releases page</a>.</p>`;
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
      ${boards.length ? `<div class="files">${boards.map(fileCard).join('')}</div>`
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
})();
