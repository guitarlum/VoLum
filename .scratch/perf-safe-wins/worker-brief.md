# Worker brief (common to every perf-safe-wins ticket)

GOAL         Implement exactly one ticket from `C:\dev\VoLum\.scratch\perf-safe-wins\issues\` (named in your
             prompt) so VoLum does less work with sound, pixels, latency and saved state unchanged.
SCOPE        Repo `C:\dev\VoLum`, branch `feature/perf-parked` (round 2; already checked out; never switch branches).
             Write only the paths the ticket lists, plus: its tests, `NeuralAmpModeler-Tests.vcxproj` /
             `tests/CMakeLists.txt` registration, `NeuralAmpModeler/installer/changelog.txt` (one line), and the
             ticket file itself (Status + a short "Result" section). Submodule tickets: the submodule named in
             the ticket, on its `volum/perf-safe-wins` branch (create it from the pinned commit if missing).
CONTEXT      Read first: `spec.md` (locked owner decisions, Definition of done, Escalate), your ticket, and the
             cited finding in `C:\dev\VoLum\.scratch\perf-sweep\findings\` plus `C:\dev\VoLum\.scratch\perf-sweep\verify.md`
             (cold-verifier conditions for your report ID). Line numbers were taken at 3a0916a8: re-find by name.
             Measurement tools: `.scratch/perf-sweep/bench/` (bench.cpp + build-bench.cmd use product codegen),
             `.scratch/perf-sweep/cpu-sample.ps1`, `VOLUM_FRAME_PERF`, `docs/screenshot-recipes.md`.
ACCEPTANCE   The ticket's Acceptance + Proof lines, and spec.md "Definition of done" 1-6.
VERIFY       `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` exit 0 (full suite, not only -Filter).
             The loop predicate is `pwsh .scratch/perf-safe-wins/verify.ps1`; it also requires every ticket resolved,
             so it will not pass until the last ticket: do not run it, the coordinator does.
FORBIDDEN    No golden/hash/tolerance/threshold change (stop and report instead). No force-push, no history
             rewrite, no edits on `main`/`dev`, no git config changes, no `git add .`. No product calls beyond
             spec.md. No fixes outside SCOPE (note them as follow-ups). No `/arch:AVX*`, `/fp:fast`, fast-math.
             Take `.scratch/1.3.0-feedback/buildlock.ps1 -Acquire -Owner perf-safe-wins` before launching VoLum.exe
             and `-Release -Owner perf-safe-wins` after.
PROCESS      Shell is Windows PowerShell 5: chain with `;` and check `$LASTEXITCODE`; no `&&`. Multi-line commit
             messages via a temp file and `git commit -F`; write that file with
             `[IO.File]::WriteAllText($f, $msg, (New-Object Text.UTF8Encoding($false)))` (no BOM: PS5
             `Set-Content -Encoding UTF8` adds one to the subject line). clang-format (18) changed C++ files before committing.
             Tests now build with /GL + incremental LTCG: if the test build hits an internal compiler error or
             a crash from a stale object, delete `NeuralAmpModeler/build-win/tests/x64/Release/int` and rebuild.
             Any render lock that hashes output: pin the Windows value from the pre-change build and print (do
             not fail) on macOS until CI pins it, as test_volum_pitch_burst.cpp does.
             Commit once for the ticket (submodule commit + push first when applicable, via the explicit HTTPS
             URL in spec.md). Commit message: plain-English summary line, short body.
CHANGELOG    State only what your measurements show, with the honest caveats (which machines, which buffer
             sizes, which sample rates, what does NOT change). No absolute promises ("never", "fits in any
             buffer"). Four earlier tickets were bounced by the verifier for over-promising.
EVIDENCE     Save the red-proof and test logs under `.scratch/perf-safe-wins/evidence/<ticket NN>/`.
REPORT       Final message (<= 25 lines): status (done / escalate + why), commit SHA(s), files changed, the exact
             commands you ran with their pass/fail, the red-proof evidence (what you perturbed, which test failed,
             then green), measured before/after numbers, follow-ups. Set the ticket `Status: resolved` only if
             everything passed; otherwise leave `Status: claimed` and explain.
