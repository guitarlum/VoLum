# Is the update notifier in this 1.3.0 book?

Type: grilling
Status: resolved

## Question

`backlog/F14-update-notifier.md` claims the design is already settled: notify-only (no auto-update), standalone + plugin, static appcast on `release: published`. Code has no client and no appcast workflow today. It pays off starting the *next* release after it ships, so slipping it burns a mute cycle.

This is not a headline and not a gate for MIDI / Pack / Chorus. Include it in 1.3.0, or park it?

Recommend: include. Small, specified, and the one leftover item that gets more expensive if delayed.

## Answer

**In 1.3.0 as must-ship**, not a headline and not first-to-cut. Cutting it burns another mute cycle: the client only notifies starting with the *next* public version.

`backlog/F14-update-notifier.md` is the contract: notify-only (browser link; no download, no in-place update), standalone + plugin, static GitHub Pages `appcast.json` published on `release: published`, 24 h machine throttle + process-wide once-guard, sidecar `volum-update-state.json` (plugins must not rewrite the main settings file), gear badge + Settings row/toggle/Check now + one-shot footer. Auto-update and code signing stay out of this book.

**Delta vs F14:** an available version is *seen* when the player uses the update row or Check now. Opening Settings only *shows* the reminder; it does not clear the badge. (F14’s “opened Settings once” would fire every time someone sets MIDI channel.)

S1 (appcast workflow + one-time GitHub Pages enable on `gh-pages`) can land early on `dev` with nothing user-visible. The client is a 1.3.0 slice. PLAY chrome must keep a settings control for the badge to ride — see [What is PLAY vs BUILD in 1.3.0?](11-play-vs-build.md).
