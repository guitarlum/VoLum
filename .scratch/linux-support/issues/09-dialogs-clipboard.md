Status: ready-for-agent
Blocked by: 07

# 09 File dialogs, clipboard, OpenURL, message boxes

## Scope

May write:
- the `iPlug2/IGraphics/Platforms/IGraphicsLinux*` dialog, clipboard and URL
  units (new files preferred)
- `NeuralAmpModeler/VoLumHarnessEnv.h`, only if Linux needs it

## Work

- **`PromptForFile(s)` and `PromptForDirectory`:** try xdg-desktop-portal via
  `gdbus`, then `zenity`, then `kdialog`.
  - Run them asynchronously, without blocking the host UI thread: the result is
    delivered on the run loop.
- **Clipboard:** X11 `CLIPBOARD` selection, with no blocking waits over 100 ms.
- **`OpenURL` and reveal:** `xdg-open`.
- **`ShowMessageBox`:** drawn by IGraphics, or `zenity`. It must never deadlock
  inside a host.

## Acceptance

- Doctest or scripted tests with fake `zenity`/`gdbus` on `PATH` (return path,
  cancel, missing tool), proving the chain order and the asynchronous delivery.
- The REAPER smoke still passes.
- Every red-proof is recorded.
