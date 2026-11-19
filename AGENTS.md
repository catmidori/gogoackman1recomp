## Agent skills

### Issue tracker

Issues live in GitHub Issues (catmidori/gogoackman1recomp), managed with the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five-role vocabulary: needs-triage, needs-info, ready-for-agent, ready-for-human, wontfix. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.

## Handoff protocol

`HANDOFF.md` at the repo root is the resume point for any new session. Read it first.
It is **local-only** (gitignored, never committed): it lives only in this working copy.

- After every completed step (every commit), update `HANDOFF.md` (state, next steps, findings, decisions). Commit the step's other files as usual; never stage `HANDOFF.md`.
- Before anything long (large build, long run, wide refactor), update `HANDOFF.md` and commit any finished work first.
- On any sign the session is ending (usage/limit warning, compaction notice, the user saying so), start nothing new: reach a consistent state, update `HANDOFF.md`, commit the finished work.
- If the tree does not build or a check fails, say so at the top of `HANDOFF.md`.

## No local paths in published Markdown

No committed `.md` file may contain paths from a developer's machine (drive paths such as `R:\…` or `C:\Users\<name>\…`, `/home/<name>/…`, `/mnt/<drive>/…`, a personal tool install location).

- In documents meant to be public, write placeholders instead: `<path-to>/Go Go Ackman (Japan).sfc`, `<project-root>`, `<msys2>`, or a project-relative path such as `rom/…`.
- A document that needs real local paths (for example `HANDOFF.md`) is local-only: list it in `.gitignore` and never stage it.
- Before every push, search the `.md` files being pushed for local paths and fix or ignore them first.
- Clearly generic examples (`C:/vcpkg/…`, `D:/packs/…`) and standard locations such as Flatpak's `~/.var/app/<app-id>/` are fine.
