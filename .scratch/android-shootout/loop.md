# android-shootout

## Goal

Three VoLum-identity Android phone-landscape UI/UX candidates (A Signal Flow, B Stage Floor, C Studio Instrument) each cover every desktop feature against a shared fake engine and carry a cold Opus `pass` pinned to their head commit, the engine spike report is accepted, and `gallery/index.html` lets the owner pick — by the 18:00 cut-off on 2026-10-20.

## Predicate

- `pwsh .scratch/android-shootout/verify.ps1` exit 0

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort android-shootout`, then `pwsh .scratch/android-shootout/verify.ps1`.
Do not trust chat memory. Continue until every Predicate line passes or the 18:00 cut-off (then run ticket 07 and stop).
Escalate and stop on new product, new sound / golden retune, irreversible git, or a real dead end.

## Verifier

- hash:
- verdict:

Per-unit acceptance lives in `verdicts/<x>.md` pinned to branch commits (`verify.ps1` checks them). This section is for the final cold review of the whole effort before stopping.

## Children

none — units are tickets in `issues/`.

## Budget and schedule

- Cut-off **18:00** local on 2026-10-20 (the overnight start was missed; the run started ~10:00). No new review round starts after 17:45. Ticket 07 starts at the latest at 18:00 and must finish by 18:30.
- At most **8 review rounds** per candidate and 4 for the design system. A unit that runs out is reported in the gallery with its last verdict. Never lower the bar.
- Box: at most 2 Gradle builds (`locks/build1|2.lock`) and 1 emulator user (`locks/emu.lock`). Laptop: at most 5 workers at once.
- Coordinator stays cheap: run this chat on a cheap model at low effort; it dispatches, records and routes. It does not read worker source files.

## Dispatch order

1. Ticket 01 (one worker). On acceptance: push, set 01 `Status: resolved`, set 02–06 `Status: ready-for-agent`.
2. Create worktrees (never by hand): `pwsh NeuralAmpModeler/scripts/worktree-win.ps1 -Add <branch> -Base feature/android-shootout -NoSubmodules` for `feature/android-design`, `feature/android-ui-a`, `feature/android-ui-b`, `feature/android-ui-c`, `feature/android-engine-spike`.
3. Spawn tickets 02, 03, 04, 05, 06 in **one message** (foreground Tasks, results return in one turn). Paths are disjoint by ticket scope.
4. When 02 is accepted: each candidate's next round starts with `git merge feature/android-design`. Reviews of a candidate only count once its head contains the accepted design commit.
5. Each round for a candidate: worker → `verify-candidate.ps1 -Candidate <x>` green → second reviewer (advisory) and Opus reviewer (gate) spawned together → verdict on disk → if `defects`, a **fresh** worker with consolidated brief (original ticket + this round's verdict + previous report path + branch).
6. Ticket 07 when every unit is accepted or at the cut-off.
7. Run the Predicate. If it fails only because of the cut-off, stop anyway and say which units are not accepted.

## Roles and models

| Role | Model | Mode |
|------|-------|------|
| Coordinator (this chat) | cheap (owner's choice) | agent |
| 01 Foundation, 02 Design, 03/04/05 Candidates A/B/C, 07 Gallery | `claude-opus-5-5-high` | agent, local |
| 06 Engine spike | `gpt-5.6-sol-high` | agent, local |

All UI work runs on Opus (owner's call: it is the strongest UI model here). The candidates differ by direction, not by model. GPT-5.6 stays the second reviewer, so a different model family still reviews every UI.
| Gate reviewer (02–05) | `claude-opus-5-5-high` | readonly, fresh, cold |
| Second reviewer (02–05, 07 ranking) | `gpt-5.6-sol-high` | readonly, fresh, cold |
| Spike verifier | `claude-opus-5-5-high` | readonly, fresh, cold |
| Ticket 01 verifier | `gpt-5.6-sol-high` | readonly, fresh, cold |

Reviewers write their verdict file themselves (they may write only `verdicts/`). If a readonly Task cannot write, it returns the verdict block and the coordinator writes it verbatim.

## Worker brief (paste verbatim, fill the brackets)

```
GOAL         [one sentence from the ticket]
SCOPE        [paths it may write; paths it must not]; branch [branch] in worktree [path]
CONTEXT      Read first, in order:
             1. c:\Users\SteffenDangmann\.agents\skills\frontend-design\SKILL.md   (UI tickets: mandatory)
             2. C:\Users\SteffenDangmann\.agents\skills\android-native-dev\SKILL.md
             3. C:\dev\VoLum\.scratch\android-shootout\spec.md   (VoLum identity overrides frontend-design's "new aesthetic")
             4. C:\dev\VoLum\.scratch\android-shootout\rubric.md   (what the reviewer will reject)
             5. C:\dev\VoLum\.scratch\android-shootout\issues\[ticket].md
             6. [previous verdict + report paths, for fix rounds]
             Shell is Windows PowerShell 5: `;` not `&&`; never `>` into files git reads; ssh traps in spec.md "Box workflow".
ACCEPTANCE   [checkable lines from the ticket]
VERIFY       pwsh NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate [x]   (exit 0 for the pushed head)
             Then git push https://github.com/guitarlum/VoLum.git [branch]
CHECKLIST    [ticket's verifier checklist] + rubric.md criteria
FORBIDDEN    no force-push, no edits outside SCOPE, no golden retune, no product calls,
             no copies of :design identity in a candidate, never touch ~/localai-platform
REPORT       full report to C:\dev\VoLum\.scratch\android-shootout\reports\[id]-r[round].md
             (what changed, screens touched, Requests for :core/:design, open risks).
             Final message at most 10 lines: status, branch + head sha, verify-candidate exit code, report path, blockers.
```

## Reviewer brief (paste verbatim)

```
You are a cold reviewer. You did not write this code and you do not edit it.
Read, in order: c:\Users\SteffenDangmann\.agents\skills\frontend-design\SKILL.md,
C:\dev\VoLum\.scratch\android-shootout\spec.md, C:\dev\VoLum\.scratch\android-shootout\rubric.md.
Candidate [x], commit [sha], round [n]. Evidence: C:\dev\VoLum\.scratch\android-shootout\evidence\[x]\[sha]\
Desktop references: C:\dev\VoLum\docs\user-guide-*.png and C:\dev\VoLum\.scratch\android-shootout\reference\
[Gate reviewer only:] Second reviewer findings: [path or "none yet"].
Look at every capture and every motion sheet. Score per rubric.md and write the verdict file it describes.
Final message: verdict, scores, blocker count, path written. At most 8 lines.
```

## Escalation (owner is asleep)

Write the question to `reports/escalations.md`, mark only the affected ticket `Status: escalated`, and continue the others. Stop the whole loop only for: irreversible git, box resource danger (Ollama or `~/localai-platform` affected, disk < 50 GB free), or the laptop losing the box for > 15 minutes.

## Ledger

<!-- append-only: timestamp | unit | what changed | predicate moved yes/no -->
- 2026-10-20 00:55 | setup | effort written, box provisioned, PoC deleted, pstack unbanned (a3b2124e) | no
- 2026-10-20 00:50 | setup | box smoke: NDK 29.0.14206865, AVD volum_phone boots in 12 s, screencap fetched; provision.sh on feature/android-shootout 412fae13 | no
- 2026-10-20 09:55 | setup | overnight start missed; cut-off moved to 18:00, candidates B and C moved to Opus (owner) | no
