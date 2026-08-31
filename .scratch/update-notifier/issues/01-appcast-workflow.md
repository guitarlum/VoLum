# Appcast workflow (S1)

Status: ready-for-agent
Blocked by: none

## Goal

Publish `appcast.json` on `release: published`. Nothing user-visible. Safe on `dev`.

## Do this

`.github/workflows/publish-appcast.yml`. Manifest shape in `.scratch/update-notifier/spec.md` / F14. Do not call GitHub Releases API from the client later — this file is the source.

If `gh-pages` cannot be enabled from the agent, still land the workflow and a sample `appcast.json` in the publish path; note the Pages enable step in `## Comments`.

## Tests

Workflow file present; JSON fixture parses with the (forthcoming) checker, or a tiny schema test if the client is not in yet. No network.

## Done when

Workflow committed. No client required.
