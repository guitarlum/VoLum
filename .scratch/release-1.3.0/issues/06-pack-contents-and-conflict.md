# What is in a Pack, and what happens on conflict?

Type: grilling
Status: open
Blocked by: 04

## Question

There is no portable export of machine settings + custom content today. `volum-settings.json`, the content library (`volum-content.json` + copied captures), and the DAW project chunk are three different stores. “Export settings” is overloaded: users mean backup of custom amps, presets, and preferences.

Prior art: `backlog/F10-settings-import-export.md`. Individual preset export in `backlog/F5-presets-full-rig.md` is the same portability problem — fold it into this decision, do not invent a second format.

Decide:

1. **Payload.** Settings only, library only, or one bundle of settings + referenced content (amps, IRs, pedals, presets)?
2. **Conflict.** Skip existing ids, merge, replace, or rename-on-import?
3. **Failure.** Transactional stage+swap, prior tree left in a backup, never clobber on a bad bundle?
4. **Older/newer.** Bundle carries a schema version; importing newer into older degrades via the tolerant reader?

Recommend: one bundle of settings + referenced content, re-key by stable id, transactional import, merge-or-skip existing ids (never silent replace of a different item with the same name). DAW chunks stay the host’s problem — a Pack is a machine/library backup, not a project file.

Depends on [How does the content library survive two writers?](04-two-writer-library.md) because import is another writer.
