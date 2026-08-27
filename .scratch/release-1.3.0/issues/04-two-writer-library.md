# How does the content library survive two writers?

Type: grilling
Status: open

## Question

Custom amps, IRs, pedals, and presets live in one process-wide `GlobalContentStore()` that writes the whole `volum-content.json` via temp+rename. There is no cross-process lock or merge. Two VoLum instances, or a DAW instance plus standalone, last-write-wins and can silently drop the other session’s library. User-visible: imported content vanishes from the registry (orphans on disk).

A Pack importer is another writer. MIDI Program Change onto a clobbered preset bank is the same wound. Prior art: `backlog/B6-multi-instance-content-library.md` and R4 phase 2 (per-instance ownership) in `backlog/R4-volum-1.2.0-content-bridge-collapse.md`.

What ownership model does 1.3.0 ship before Pack?

- Single-owner (refuse the second writer),
- Locked read-modify-write merge,
- Or per-item files?

Recommend: decide this before Pack format. Pair the same ownership story with [What happens when we delete content that is currently playing?](05-delete-while-playing.md). Do not silently skip this gate — code still has the defect.
