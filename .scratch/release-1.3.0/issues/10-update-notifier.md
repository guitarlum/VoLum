# Is the update notifier in this 1.3.0 book?

Type: grilling
Status: open

## Question

`backlog/F14-update-notifier.md` claims the design is already settled: notify-only (no auto-update), standalone + plugin, static appcast on `release: published`. Code has no client and no appcast workflow today. It pays off starting the *next* release after it ships, so slipping it burns a mute cycle.

This is not a headline and not a gate for MIDI / Pack / Chorus. Include it in 1.3.0, or park it?

Recommend: include. Small, specified, and the one item on the leftover list that gets more expensive if delayed.
