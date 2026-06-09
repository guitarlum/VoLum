# D1 — Input level guidance (+4 dBu) and small docs polish

Plan a docs-only update. Add an explicit input-level recommendation (+4 dBu) to the user guide so users stop guessing. While in there, audit related sections for the same questions: standalone vs plugin behavior, where audio settings live (cross-link with U1 outcome), buffer-size guidance (cross-link with B1 outcome), input/output device guidance (cross-link with B5 outcome). Produce a docs ticket with: exact sections to edit in `docs/user-guide.en.md` and `docs/user-guide.de.md`, any screenshots that need refreshing (`docs/user-guide-*.png`), and a changelog line if user-visible. Do not implement.

Work must happen on a dedicated feature branch off the latest `dev`, named `feature/docs-input-level-and-polish`. Do not commit to `dev` or `main` directly. The branch is merged back into `dev` only after the ticket's acceptance criteria are met and tests/docs/changelog are in place. Never promote to `main` outside of a release.
