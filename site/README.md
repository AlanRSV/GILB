# GILB download site

A one-page site where people download the newest GILB firmware. It never stores the images itself: it
shows the newest **GitHub release** of this repository and links its files, so publishing a release
updates the site with no redeploy.

- `server.js`: a dependency-free Node server (Node 18+). It serves `public/` and `/api/latest`, the
  newest release (its name, date, notes, files, sizes and SHA-256), cached for 10 minutes so visitors
  never meet GitHub's API limit (60 requests an hour without a token).
- `public/`: the page (`index.html`, `style.css`, `app.js`) and four screenshots from `docs/images/`.

The page offers the files by name: `HIFIEC90.IMG` (8 GB board) and `MINIV900.IMG` (512 MB board). Any
other file attached to a release is listed under them.

## Publishing a firmware release

1. Build the image (`handoff.md`, Build): `build/HIFIEC90.IMG` (and `build/MINIV900.IMG` for the 512 MB
   board, `BOARD=512m`).
2. On GitHub: **Releases › Draft a new release**, a tag such as `v2026.11.25`, a title, notes (shown on
   the site under "What's new"), attach the `.IMG` files, **Publish release**.
3. The site shows it within 10 minutes (the cache), or at once after a restart.

## Hosting on Railway

1. **New Project › Deploy from GitHub repo**, pick this repository.
2. In the service's **Settings**: **Root Directory** `site`. Railway sees `package.json` and runs
   `npm start` (no install step needed: there are no dependencies).
3. **Settings › Networking › Generate Domain** (or add your own). Railway sets `PORT`; the server uses it.
4. Optional **Variables**: `GITHUB_TOKEN` (a fine-grained token with no permissions: public repos only
   need it for a higher API limit), `REPO` (default `AlanRSV/GILB`), `CACHE_MINUTES` (default 10).
   Health check path: `/healthz`.

## Running it locally

```
cd site
node server.js            # http://localhost:3000
```

`MOCK_RELEASE=test/release.json node server.js` serves a made-up release from a file instead of GitHub
(to try the page before a real release exists).
