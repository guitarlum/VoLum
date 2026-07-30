# B8 — The Windows symbols do not match the build most users run

Found while debugging the 1.2.1 shutdown hang from a real user install.
Debuggability only; there is no user-visible defect. Ledger:
`backlog/1.2.1-audit-deferred.md`.

## What happens

Both CI (`.github/workflows/ci.yml`) and the release workflow
(`.github/workflows/release-native.yml`) build Windows twice, in this order:

```
makedist-win.bat full installer
makedist-win.bat full zip
```

`makedist-win.bat` runs `update_installer-win.py` first, and that script rewrites
`resources/main.rc` and `installer/VoLum.iss` in place on every invocation —
`fileinput.input(..., inplace=1)` replaces the file whether or not the content
changed. The new timestamp makes MSBuild recompile the resource and relink, so
the second run produces a *different* executable with a *different* CodeView GUID
from the one Inno Setup already packaged.

Result: `VoLum-v1.2.1-win-pdbs.zip` matches the portable zip's binaries and not
the installer's. Verified on CI run 30499977446 — the portable `VoLum_x64.exe`
(GUID `bb466ee5-101c-4d33-8c8e-d2c0289cbe14`) and `VoLum.vst3`
(`765e1217-4366-4481-8b03-b9f3e976f8d9`) both resolve against the shipped PDBs,
while the installed executable from the same run did not.

## Why it is low priority

PDBs are not a release asset. `release-native.yml` uploads only the setup
executable and the portable zip, so no user ever receives a mismatched pair. The
cost is paid by us: a crash dump from an installed build will not symbolize
against the artifact PDBs, which cost real time on the 1.2.1 hang before the
mismatch was noticed.

## Fix

Make `update_installer-win.py` write only when the content actually changes.
Read the file, compute the new text, compare, and skip the write if identical.
That removes the spurious relink, so the installer and the portable zip ship the
same binaries and one PDB set matches both. It also shortens the Windows CI job.

Keep the guarantee the current script exists for: `resources/main.rc` must stay
in lockstep with `config.h`, which is why it is rewritten at all
(changelog 07/10/2026). A write-if-changed version preserves that;
`smoke-upgrade-install-win.ps1` already reads `ProductVersion` back off the
installed executable, so a stale resource would still be caught.

## Acceptance criteria

- Running `update_installer-win.py` twice with no config change leaves both
  `resources/main.rc` and `installer/VoLum.iss` byte-identical *and*
  timestamp-identical.
- After `makedist-win.bat full installer` followed by
  `makedist-win.bat full zip`, the installer's `VoLum.exe` and the portable
  `VoLum_x64.exe` have the same CodeView GUID.
- A packaging check asserts that the CodeView GUID of every shipped binary occurs
  in the shipped PDB set. Cheap to write: read the PE debug directory's RSDS
  record and search the PDB for those 16 bytes. Wire it into
  `verify-packaging-win.ps1`.
