# GILB download site

A one-page site where people download the newest GILB firmware. It never stores the images itself: it
shows the newest **GitHub release** of this repository and links its files, so publishing a release
updates the site with no redeploy.

- `server.js`: a dependency-free Node server (Node 18+). It serves `public/`, `/api/latest`, the
  newest release (its name, date, notes, files, sizes and SHA-256), and `/api/betas`, the newest
  pre-releases (3 by default), each cached for 10 minutes so visitors never meet GitHub's API limit
  (60 requests an hour without a token).
- `public/`: the page (`index.html`, `style.css`, `app.js`) and four screenshots from `docs/images/`.

The page offers the files by name: `HIFIEC90.IMG` (8 GB board) and `MINIV900.IMG` (512 MB board, still
being tested: published only as a beta, and the page says so). Any
other file attached to a release is listed under them.

## Publishing a firmware release

1. Build the image (`handoff.md`, Build): `build/HIFIEC90.IMG` (and `build/MINIV900.IMG` for the 512 MB
   board, `BOARD=512m`).
2. On GitHub: **Releases › Draft a new release**, a tag such as `v2026.11.25`, a title, notes (shown on
   the site under "What's new"), attach the `.IMG` files, **Publish release**.
3. The site shows it within 10 minutes (the cache), or at once after a restart.

## Publishing a beta

A beta is a GitHub **pre-release**: draft the release as above, tick **Set as a pre-release**, publish.
It shows under **Beta builds**, with a warning that it is for testing, and never as the main download
(GitHub's "latest" release is never a pre-release). The section stays hidden while there is none.
Betas with Settings › System › Programs are built with `PROGRAMS=1` (`docs/PROGRAMS.md`).

## Hosting on Railway

1. **New Project › Deploy from GitHub repo**, pick this repository.
2. In the service's **Settings**: **Root Directory** `site`. Railway sees `package.json` and runs
   `npm start` (no install step needed: there are no dependencies).
3. **Settings › Networking › Generate Domain** (or add your own). Railway sets `PORT`; the server uses it.
4. **Variables**: `GITHUB_TOKEN`, recommended: without it GitHub's API allows 60 requests an hour per
   IP address, and Railway shares its outgoing IP with other apps, so it answered 403 before the site
   asked once (2026-11-26). The page then still offers the 8 GB image through GitHub's direct link,
   without its size, checksum or notes. A fine-grained token with no permissions is enough (a public
   repo only needs it for the higher limit: 5000 an hour). Optional: `REPO` (default `AlanRSV/GILB`),
   `CACHE_MINUTES` (default 10), `RETRY_MINUTES` (after a refusal, default 2), `BETAS` (how many
   betas to list, default 3).
   Health check path: `/healthz`.

## Running it locally

```
cd site
node server.js            # http://localhost:3000
```

`MOCK_RELEASE=test/release.json MOCK_BETAS=test/betas.json node server.js` serves a made-up release
and two made-up betas from files instead of GitHub (to try the page before a real release exists).
