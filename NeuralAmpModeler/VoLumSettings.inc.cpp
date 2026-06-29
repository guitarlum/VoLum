// VoLum settings persistence + per-amp / per-mode snapshot helpers.
//
// Tail-included from NeuralAmpModeler.cpp (NOT a separate TU); pure file-size
// hygiene split. This is now a thin umbrella: it pulls in VoLumPrePostLock.h once
// and then tail-#includes the three concern-split siblings below (locks/dirty,
// scene apply + file I/O, preset bank). All are part of the same TU.

#include "VoLumPrePostLock.h"

#include "VoLumSettingsLocks.inc.cpp"
#include "VoLumSettingsScene.inc.cpp"
#include "VoLumSettingsPresets.inc.cpp"
