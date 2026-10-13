# 1.3.0 CI e2e, morning of 12 Oct 2026

The 08:00 note below is the cut-off snapshot. The owner then gave two more hours. At 09:47 `pwsh .scratch/1.3.0-ci-e2e/verify.ps1` exited 0 on `dev` `b6ea72f7`. Dev CI [38214773536](https://github.com/guitarlum/VoLum/actions/runs/38214773536) is green, including `Windows installer upgrade`, `macOS installer upgrade`, and `macOS standalone e2e`. That run uploaded `mac-e2e-shots`.

## Cut-off snapshot (08:00)

`dev` was `fc1db78f`. Newest dev CI then was [38202235306](https://github.com/guitarlum/VoLum/actions/runs/38202235306), success. Both installer jobs were green:

| Job | Result |
| --- | --- |
| Windows installer upgrade | success |
| macOS installer upgrade | success |

Ticket 01 first went green on [38180953415](https://github.com/guitarlum/VoLum/actions/runs/38180953415) (`47b89fae`): `NEGATIVE CONTROL OK`, `ALL PASS 93 checks`. Ticket 02 first went green on [38198542144](https://github.com/guitarlum/VoLum/actions/runs/38198542144) (`0c6f6b9a`): `NEGATIVE CONTROL OK`, `ALL PASS 107 checks`. A cold verifier from another model family accepted both diffs. They are merged. Ticket 01's dev CI after merge was [38184779812](https://github.com/guitarlum/VoLum/actions/runs/38184779812) (`c3f1e3e8`), success.

On the green dev run, `macOS installer upgrade` (job 114667917382) recorded Gatekeeper and did not assert it:

```
spctl --status
assessments enabled
spctl --assess --type open --context context:primary-signature -vvv VoLum-installer.dmg
VoLum-installer.dmg: rejected
spctl --assess --type install -vvv 'VoLum Installer.pkg'
VoLum Installer.pkg: rejected
spctl --assess --type exec -vvv VoLum.app
VoLum.app: rejected
```

The same three rejects, with `source=no usable signature` and exit 3, were on the ticket 02 run [38198542144](https://github.com/guitarlum/VoLum/actions/runs/38198542144). That is the expected ad-hoc, un-notarized build.

## Not green

Ticket 03 is still claimed. It is not on `origin/dev`, so verify.ps1 also reports a missing `macOS standalone e2e` job and a missing `mac-e2e-shots` artifact on the dev run.

Last completed feature run: [38205604479](https://github.com/guitarlum/VoLum/actions/runs/38205604479) on `3415685d`, failure. Every job succeeded except `macOS standalone e2e`, which logged `NEGATIVE CONTROL OK` and ended `FAILED 1 of 84 checks`. VoLum did migrate the rewound library (schema 4, IR trim, pre-1.2.1 backup). The script counted readable IRs from a copy of the library file kept outside the content folder, so it expected no rewrite.

Screenshots from that run are artifact `mac-e2e-shots` (about 747 KB): [run 38205604479](https://github.com/guitarlum/VoLum/actions/runs/38205604479). The worker reported five non-blank captures (PLAY, BUILD, Settings SIGNAL / MIDI / SYSTEM), no alert and no crash.

The fix is pushed on `feature/ci-e2e-03` at `09ca2eb9` (count IRs in the real content folder, and keep the shots temp home out of the upload). It was ready after the 07:30 dispatch limit in the worker brief. The coordinator dispatched it at 07:48 as [38209565384](https://github.com/guitarlum/VoLum/actions/runs/38209565384) and did not wait. That run is not verified and the branch is not merged.

## Findings

- F-113 in `.scratch/1.3.0-hunt/findings.md`: the first 1.3.0 macOS launch over a 1.2.x `settings.ini` fails to open audio (`the device (0) does not support the requested channel count`) and shows an Audio Error. The window still appears and the process quits with no crash report. Deferred to 1.3.1.
- Pluginval on the macOS job failed twice on run [38192562105](https://github.com/guitarlum/VoLum/actions/runs/38192562105) (exit 9, then an Apple virtual GPU assert) and passed on the next dispatch of the same commit. Not a product change.

## Left for the next session

Wait for [38209565384](https://github.com/guitarlum/VoLum/actions/runs/38209565384). If it is green, including `ALL PASS` and `mac-e2e-shots`, cold-verify `09ca2eb9` and merge with `merge-verified-branch.ps1`. Do not `-Dispatch` `dev`; the merge push starts that CI. Then rerun `verify.ps1`.
