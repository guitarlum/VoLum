Status: ready-for-agent
Blocked by: 12, 13, 14

# 15 Docs, changelog, release notes, final predicate

## Scope

May write:
- `docs/user-guide.en.md` and `docs/user-guide.de.md`, kept in sync
- `README.md` and `README.de.md`, replacing the yabridge note
- `NeuralAmpModeler/installer/changelog.txt`
- `.scratch/linux-support/reports/release-notes-1.4.0-beta.md`
- delete `backlog/O1-linux-support-spike.md` and its `backlog/README.md` rows

## Work

- **User guide (EN/DE):** a Linux install section covering the tarball,
  `install.sh`, JACK, PipeWire and Pulse; a known-limitations section
  (beta, X11/XWayland, latency not certified); and the Linux library path.
- **Changelog:** one line per milestone already merged, if not yet present, and
  a 1.4.0 beta Linux line.
- **Release notes:** draft the 1.4.0-beta Linux notes in project style. The owner
  publishes them.
- **Final predicate:** run `verify.ps1 -Final` and get a cold verifier `accepted`
  at the current hash. Final merge into `dev`.

## Acceptance

`verify.ps1 -Final` exits 0, and loop.md `## Verifier` shows `accepted` at the
current `worktree_hash`.
