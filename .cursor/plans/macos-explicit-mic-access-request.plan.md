---
name: macos-explicit-mic-access-request
overview: Deferred backlog. Add AVCaptureDevice.requestAccess(for:.audio) on macOS standalone startup with re-probe on grant.
---

# macOS explicit mic access request (deferred)

Add `AVCaptureDevice requestAccess(for:.audio)` early in `IPlugAPP_main.cpp` `SWELLAPP_ONLOAD`, with completion handler that re-runs `ProbeAudioIO()`.

Current implicit RtAudio CoreAudio probe path is empirically working on fresh installs; this is belt-and-suspenders for future macOS behavior changes.
