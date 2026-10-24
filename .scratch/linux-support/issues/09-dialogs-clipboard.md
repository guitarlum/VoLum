Status: claimed
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

## Carried from ticket 07 (coordinator, 2026-10-24)

- `iPlug2/IPlug/VST3/IPlugVST3.cpp:162` has a blank line with trailing spaces
  from 07; remove it in your iPlug2 commit.
- 07 left `CreatePlatformTextEntry` / popup menus as they are in
  `IGraphicsLinux.cpp`; check that text entry and the VoLum popup menus work
  in REAPER while you are there, and file what does not under 13.

## Acceptance

- Doctest or scripted tests with fake `zenity`/`gdbus` on `PATH` (return path,
  cancel, missing tool), proving the chain order and the asynchronous delivery.
- The REAPER smoke still passes.
- Every red-proof is recorded.
