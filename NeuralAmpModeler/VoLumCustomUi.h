#pragma once

// VoLum 1.2.0 BYO + presets UI shells (unified dropdown + Manage design).
//
// Controls, rendering against the registry-backed VoLumCustomContentApi:
//   - VoLumPresetBarControl    : F5 preset strip centred in the top header
//                                (prev/name/next + opens the preset dropdown).
//   - VoLumListMenuControl     : reusable anchored dropdown (item list + a single
//                                "Manage..." action). Used for presets and IR cabs.
//   - VoLumCustomOverlayControl: one full-window overlay with two screens -
//       * Manage  : a shared CRUD list for presets / custom IRs / custom pedals
//                   (Rename / Delete + add: Save-as-new/Update for presets, or
//                   Import via OS file dialog for IRs/pedals).
//       * Builder : file-first custom amp create/edit (DIRECT + 3 stock cabs,
//                   numbered channels) with a live (speaker x channel) grid.
//
// The PRE pedal dropdown (custom pedals + "Manage custom pedals...") lives in
// VoLumTriptychMenus.h. Load/save/import are live against the content registry.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentApi.h"
#include "VoLumFractalArt.h"
#include "VoLumIrFileGuard.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "VoLumPresetBar.h"
#include "VoLumListMenu.h"
#include "VoLumConfirmDialog.h"
#include "VoLumNameDialog.h"

#include "VoLumCustomOverlay.h"
