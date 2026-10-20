# android-shootout — feature inventory

Every user-facing control, mode, dialog, list, setting, state and recovery path
of desktop VoLum 1.3 (`docs/user-guide.en.md` + the desktop code), one row each,
with its Android mapping. `:core` checks its `FeatureRegistry` against this table
(`FeaturesMarkdownTest`): ids, screen ids, interactions and N/A rows must match.

Columns: `id | desktop source | Android mapping | screen | interaction | N/A reason`.

Interactions, as the harness coverage test operates them (semantics only, never
the reducer):

| interaction | how the harness operates the `featureId` node | passes when |
| --- | --- | --- |
| `tap` / `toggle` / `select` | click (touch at the node centre, after `performScrollTo` when it scrolls) | the feature's state check holds |
| `drag` | `SetProgress` to 15 % or 85 % of the node's `ProgressBarRangeInfo` (whichever is farther); a swipe when the node has no `SetProgress` | the bound value moved toward the target |
| `text` | `SetText` with the registry's sample text, then the IME action when present | the state check holds |
| `file` | click; the fake engine answers the Storage Access Framework pick with fixture files | the state check holds |
| `action:<label>` | the node's `CustomAccessibilityAction` with that label | the state check holds |
| `view` | none; presence only | a displayed node carries the id |
| `engine` | none in the UI; the fake engine / reducer behaviour is pinned by `:core` unit tests | the unit test passes |

When several nodes carry one id (list rows, channel options), the harness tries
them in order, from a fresh fixture each time, until one satisfies the check.
Prev / Next controls (`header.preset.*`, `play.sound.*`, `midi.bank.*`) pass only
when they land where the desktop steps in their own direction: presets and Sounds
wrap at both ends, MIDI banks stop at the first and last bank. A control that
steps the other way fails.
Screen ids and fixtures live in `:core` (`ScreenRegistry`, `Fixtures`); the
fixture each feature starts from is in `FeatureRegistry`.

## Navigation and header

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| nav.view.build | §PLAY, §Quick Start 3 / `kCtrlTagVoLumModeToggle` (P) | PLAY/BUILD switch, from PLAY | play | tap | |
| nav.view.play | §PLAY / `kCtrlTagVoLumModeToggle` | PLAY/BUILD switch, from BUILD | build.amp | tap | |
| nav.section.pre | §The BUILD Screen 5 / PRE strip (`1`) | PRE tab of the chain strip | build.amp | select | |
| nav.section.amp | §The BUILD Screen 5 / AMP strip (`2`) | AMP tab of the chain strip | build.pre | select | |
| nav.section.post | §The BUILD Screen 5 / POST strip (`3`) | POST tab of the chain strip | build.amp | select | |
| nav.tuner.open | §Tuner And Metronome / `kCtrlTagVoLumTuner` (T) | toolbar tuner button | play | tap | |
| nav.metronome.open | §Tuner And Metronome / `kCtrlTagVoLumMetronomeButton` (M) | toolbar metronome button | play | tap | |
| nav.settings.open | §Settings / gear `kCtrlTagSettingsBox` (H) | toolbar gear | play | tap | |
| nav.audio_status.open | §Troubleshooting "Interface unplugged" | input-state indicator in the toolbar opens the audio status panel | play | tap | |
| nav.back | §Keyboard Shortcuts `Esc` | system Back closes the top panel; every panel also has its own close control (listed per panel) | - | - | Esc is a keyboard key; Android Back is a system gesture the harness cannot operate through semantics, and every panel's close control is its own feature id |
| header.preset_name | §The BUILD Screen "preset name sits in the middle of the header" / `kCtrlTagVoLumPresetBar` | current preset name in the BUILD header | build.amp | view | |
| header.preset.prev | §The BUILD Screen `<` / `kCtrlTagVoLumPresetBar` | previous preset | build.amp | tap | |
| header.preset.next | §The BUILD Screen `>` / `kCtrlTagVoLumPresetBar` | next preset | build.amp | tap | |
| header.preset.menu | §The Preset Menu "Click the preset name" / `kCtrlTagVoLumPresetMenu` | tap the preset name opens the preset menu | build.amp | tap | |
| header.unsaved | §The Preset Menu "(unsaved)" | "(unsaved)" mark beside the preset name | build.amp | view | |
| meters.in | §PLAY/§BUILD / `kCtrlTagInputMeter` | IN meter (fake engine level) | build.amp | view | |
| meters.out | §PLAY/§BUILD / `kCtrlTagOutputMeter`, `kCtrlTagOutputMeterR` | OUT meter (fake engine level) | build.amp | view | |
| status.output_safety | §Troubleshooting "Output safety active" / `kCtrlTagVoLumFooter` | output-safety notice in BUILD | build.amp | view | |
| banner.error | §Your Own Amps "File is not a WAV file", model load failure / `kMsgTagLoadFailed` | error banner (load or import failure) | build.amp | view | |
| banner.dismiss | `kMsgTagLoadFailed` banner | dismiss the error banner | build.amp | tap | |
| tip.build | §Quick Start 3 "Tweak your sound" / `kCtrlTagVoLumBuildTip` | first-launch note pointing at the BUILD switch | play | view | |
| dialog.confirm.message | §Deleting Content "VoLum asks first" / `kCtrlTagVoLumConfirm` | confirm dialog text naming the item | presets.manage | view | |
| dialog.confirm.accept | `kCtrlTagVoLumConfirm` | confirm button | presets.manage | tap | |
| dialog.confirm.cancel | `kCtrlTagVoLumConfirm` | cancel button | presets.manage | tap | |

## PLAY

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| play.sound.recall | §Play Your Sounds "Recall a Sound" / `VoLumPlaySurface` rows | tap a Sound row | play | tap | |
| play.sound.next | §Keyboard Shortcuts PLAY `Down`/`Right` | next-Sound control sized for arm's length | play | tap | |
| play.sound.prev | §Keyboard Shortcuts PLAY `Up`/`Left` | previous-Sound control | play | tap | |
| play.sound.live | §Play Your Sounds "LIVE marks the Sound you recalled last" | LIVE mark on the recalled Sound | play | view | |
| play.sound.name | §PLAY / PLAY name banner (Poiret One) | current Sound name | play | view | |
| play.sound.program | §PLAY "program number from 0 to 127" | program number on each Sound row | play | view | |
| play.sound.invalid | §The Preset Menu "Invalid slot" | "Invalid slot" row state after its preset was deleted | play | view | |
| play.sound.replace | §Add, Replace And Arrange "Replace: double-click a row" | replace control on a row opens the picker for that program | play | tap | |
| play.sound.clear | §Add, Replace And Arrange "Clear: click its ×" | clear control on a row | play | tap | |
| play.sound.reorder | §Add, Replace And Arrange "Reorder: drag a row" / `win-drag` UAT 3b | drag to reorder; also the row's "Move down" accessibility action | play | action:Move down | |
| play.add_this | §Add, Replace And Arrange "+ Add this sound" | "+ Add this sound" | play | tap | |
| play.add_this.unnamed | §Add "If it has no name yet, the name box opens first" | "+ Add this sound" on an unnamed rig opens the name box | play | tap | |
| play.add_this.full | §Add "when all 128 are taken, it opens the picker" | "+ Add this sound" with 128 Sounds opens the picker | play | tap | |
| play.add_sound | §Add "+ Add Sound opens the picker" | "+ Add Sound" | play | tap | |
| play.save | §Play Your Sounds "Tweak and save ... Ctrl+S" | save control on PLAY opens the name box | play | tap | |
| play.stomp.pitch | §Play Your Sounds "Stomps" (1) / `kPlayBypassParamNames` `kPrePitchActive` | Pitch stomp | play | toggle | |
| play.stomp.comp | §Play Your Sounds "Stomps" (2) / `kPreCompActive` | Comp stomp | play | toggle | |
| play.stomp.nam1 | §Play Your Sounds "Stomps" (3) / `kPreNam1Active` | NAM 1 stomp | play | toggle | |
| play.stomp.nam2 | §Play Your Sounds "Stomps" (4) / `kPreNam2Active` | NAM 2 stomp | play | toggle | |
| play.stomp.chorus | §Play Your Sounds "Stomps" (5) / `kChorusActive` | Chorus stomp | play | toggle | |
| play.stomp.delay | §Play Your Sounds "Stomps" (6) / `kDelayActive` | Delay stomp | play | toggle | |
| play.stomp.reverb | §Play Your Sounds "Stomps" (7) / `kReverbActive` | Reverb stomp | play | toggle | |
| play.stomp.trem | §Play Your Sounds "Stomps" (8) / `kTremoloActive` | Tremolo stomp | play | toggle | |
| play.stomp.edit | §Play Your Sounds "Right-click a stomp to edit that pedal in BUILD" | visible edit affordance on a stomp opens that pedal in BUILD (long-press may add, never replace) | play | tap | |
| play.art | §Moving Art / `VoLumArtAnimator`, `VoLumStageArtCache` | moving amp art driven by fake-engine energy | play | view | |
| play.meters | §3b "PLAY IN/OUT" / PLAY meters | PLAY IN/OUT meters | play | view | |
| play.picker.program | §Add "choose a PROGRAM number" | program number chooser in the picker | play.picker | select | |
| play.picker.factory | §Add "then a Factory or User preset" | Factory preset rows in the picker | play.picker | tap | |
| play.picker.user | §Add "then a Factory or User preset" | User preset rows in the picker | play.picker | tap | |
| play.picker.close | §Keyboard Shortcuts `Esc` | close the picker | play.picker | tap | |
| play.starting_sounds | §The Five Starting Sounds | first library open fills programs 0-4 once | - | engine | |
| play.view_memory | §PLAY "remembers the view you left" | last view restored on launch | - | engine | |

## BUILD — amps and cabs

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| amp.list.select | §Amps And Cabs "Pick an amp in the left list" / `kCtrlTagVoLumAmpList` | amp list row | amp.picker | tap | |
| amp.list.select_custom | §The BUILD Screen 1 "your own amps under CUSTOM" / `kCtrlTagVoLumAmpList` CUSTOM rows, `mVolumCustomMainIdx` | custom amp row plays that amp | amp.picker | tap | |
| amp.list.custom | §The BUILD Screen 1 "your own amps under CUSTOM" | CUSTOM group in the amp list (the list may be its own panel or shown in place; reachability checks it is at most 3 taps from home) | amp.picker | view | |
| amp.name | §The BUILD Screen 2 / `kCtrlTagVoLumAmpName`, `VoLumHero` | amp title (Poiret One) on the hero art | build.amp | view | |
| amp.hero | §The BUILD Screen 2 / `kCtrlTagVoLumHeroImage`, `VoLumHero.h` | sunset hero / per-amp art | build.amp | view | |
| amp.channel | §Amps And Cabs "Channels" / `kCtrlTagVoLumChannelStep` | channel stepper or segmented choice | build.amp | select | |
| amp.cab.none | §Amps And Cabs "Cabs: No Cab" / `kCtrlTagVoLumSpeakerRow` | No Cab | build.amp | select | |
| amp.cab.g12 | §Amps And Cabs "G12" | G12 | build.amp | select | |
| amp.cab.g65 | §Amps And Cabs "G65" | G65 | build.amp | select | |
| amp.cab.v30 | §Amps And Cabs "V30" | V30 | build.amp | select | |
| amp.cab.custom_ir | §Custom IRs "Click Custom IR in the cab row" / `kCtrlTagVoLumIrMenu` | Custom IR opens the IR menu | build.amp | tap | |
| amp.cab.custom_ir_disabled | §Custom IRs "On other channels the Custom IR button is greyed out" | Custom IR shown disabled on a channel without a No Cab capture | build.amp | view | |
| amp.knob.input | §Amps And Cabs "INPUT" / `kInputLevel` | INPUT knob | build.amp | drag | |
| amp.knob.gate | §Amps And Cabs "GATE" / `kNoiseGateThreshold` | GATE knob | build.amp | drag | |
| amp.knob.bass | §Amps And Cabs "BASS" / `kToneBass` | BASS knob | build.amp | drag | |
| amp.knob.mid | §Amps And Cabs "MID" / `kToneMid` | MID knob | build.amp | drag | |
| amp.knob.treble | §Amps And Cabs "TREBLE" / `kToneTreble` | TREBLE knob | build.amp | drag | |
| amp.knob.output | §Amps And Cabs "OUTPUT ... fully down mutes" / `kOutputLevel` | OUTPUT knob (bottom reads −∞) | build.amp | drag | |
| amp.knob.reset | §The BUILD Screen "Double-click resets a knob" | the knob's "Reset" accessibility action (plus double-tap) on AMP BASS; every knob offers it | build.amp | action:Reset | |
| amp.knob.exact | §Keyboard Shortcuts selected knob "Enter: Type an exact value" / `kCtrlTagVoLumExactEntry` | type an exact value on AMP BASS (SetText); every knob offers it | build.amp | text | |
| amp.gate.toggle | `kNoiseGateActive` / `MAIN_LANE_TOGGLES` "NOISE GATE" | NOISE GATE switch | build.amp | toggle | |
| amp.eq.toggle | `kEQActive` / `MAIN_LANE_TOGGLES` "EQ" | EQ switch | build.amp | toggle | |
| amp.rig_memory | §Each Amp Remembers Its Rig | cab, channel, knobs, PRE, POST, Dual Amp kept per amp | - | engine | |
| amp.lite_note | §Amps And Cabs "If your computer struggles, try LITE" | LITE lives in SIGNAL (signal.perf.lite) | - | engine | |

## BUILD — Dual Amp

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| dual.toggle | §Dual Amp 2 "Switch to Dual Amp" / `kDualAmpActive` | Dual Amp switch | build.amp | toggle | |
| dual.focus.main | §Dual Amp 4 "Click MAIN" (Tab) | MAIN lane choice | build.amp | select | |
| dual.focus.support | §Dual Amp 4 "Click ... SUPPORT" (Tab) | SUPPORT lane choice | build.amp | select | |
| dual.support.choose | §Dual Amp 3 "Choose support amp" / `kCtrlTagVoLumSupportAmpMenu` | empty SUPPORT lane opens the support amp list | build.amp | tap | |
| dual.support.select | §Dual Amp 3 / `kSupportAmpIdx` | support amp row | support.amp_menu | tap | |
| dual.support.select_custom | §Dual Amp 3 + §The BUILD Screen 1 "your own amps under CUSTOM" / `_VolumShowSupportAmpMenu` CUSTOM group, `mVolumCustomSupportIdx` | custom amp row in the support amp list | support.amp_menu | tap | |
| dual.support.none | §Dual Amp 3 "(none) empties it again" | (none) row | support.amp_menu | tap | |
| dual.support.close | §Keyboard Shortcuts `Esc` | close the support amp list | support.amp_menu | tap | |
| dual.pan.main | §Dual Amp "a small PAN knob" / `kMainAmpPan` | MAIN PAN knob | build.amp | drag | |
| dual.pan.support | §Dual Amp / `kSupportAmpPan` | SUPPORT PAN knob | build.amp | drag | |
| dual.polarity | §Dual Amp "Ø (polarity) button, on by default" | SUPPORT Ø switch | build.amp | toggle | |
| dual.support.channel | §Dual Amp "Each side has its own channel" / `kSupportChannelIdx`, `kCtrlTagVoLumSupportChannelStep` | SUPPORT channel | build.amp | select | |
| dual.support.cab | §Dual Amp "cab or IR" / `kSupportSpeakerIdx` | SUPPORT stock cab choice | build.amp | select | |
| dual.support.cab.custom_ir | §Dual Amp "Each side has its own channel, cab or IR" + §Custom IRs "Click Custom IR in the cab row" / `kCtrlTagVoLumSpeakerRow` with SUPPORT focused, `kCtrlTagVoLumIrMenu` | Custom IR in the SUPPORT cab choice opens the IR menu for SUPPORT | build.amp | tap | |
| dual.support.cab.custom_ir_disabled | §Custom IRs "On other channels the Custom IR button is greyed out", on a custom SUPPORT amp / `volum::custom::ChannelHasDirect` | SUPPORT Custom IR shown disabled on a channel without a No Cab capture | build.amp | view | |
| dual.support.ir.select | §Dual Amp "cab or IR" + §Custom IRs 3 "Pick the IR from the Custom IR menu" / `kSupportIRToggle`, scene `supportActiveIrId` (the menu acts on the focused lane) | IR row picked while SUPPORT is focused plays on SUPPORT; MAIN keeps its cab and IR | cab.ir_menu | tap | |
| dual.support.knob.input | §Dual Amp "knobs" / `kSupportInputLevel` | SUPPORT INPUT | build.amp | drag | |
| dual.support.knob.gate | `kSupportNoiseGateThreshold` | SUPPORT GATE | build.amp | drag | |
| dual.support.knob.bass | `kSupportToneBass` | SUPPORT BASS | build.amp | drag | |
| dual.support.knob.mid | `kSupportToneMid` | SUPPORT MID | build.amp | drag | |
| dual.support.knob.treble | `kSupportToneTreble` | SUPPORT TREBLE | build.amp | drag | |
| dual.support.knob.output | `kSupportOutputLevel` | SUPPORT OUTPUT | build.amp | drag | |
| dual.support.gate.toggle | `kSupportNoiseGateActive` / `SUPPORT_LANE_TOGGLES` | SUPPORT NOISE GATE switch | build.amp | toggle | |
| dual.support.eq.toggle | `kSupportEQActive` / `SUPPORT_LANE_TOGGLES` | SUPPORT EQ switch | build.amp | toggle | |
| dual.route | `kDualAmpRoute` (STACK, L/R, CUSTOM) | not surfaced; CUSTOM with per-lane PAN | - | - | the desktop UI no longer surfaces STACK / L-R; the enum only keeps old chunks loading |

## BUILD — PRE pedals

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| pre.lock | §Each Amp Remembers Its Rig "click the lock in the PRE ... header" / `PADLOCK_*` | PRE lock | build.pre | toggle | |
| pre.store | §Each Amp Remembers Its Rig "A Store arrow appears" / `STORE_TO_AMP_FN` | Store arrow when locked pedals differ | build.pre | tap | |
| pre.lock.play_unlocks | §Each Amp Remembers Its Rig "Switching to PLAY ... stores the locked section ... and unlocks" | same on Android | - | engine | |
| pre.card.pitch | §PRE Pedals "click a pedal card" / `kCtrlTagVoLumPitchCard` | Pitch card | build.pre | select | |
| pre.card.comp | §PRE Pedals / `kCtrlTagVoLumCompCard` | Comp card | build.pre | select | |
| pre.card.nam1 | §PRE Pedals / `kCtrlTagVoLumPreNam1Card` | NAM 1 card | build.pre | select | |
| pre.card.nam2 | §PRE Pedals / `kCtrlTagVoLumPreNam2Card` | NAM 2 card | build.pre | select | |
| pre.pitch.on | §PRE Pedals "switch it on with its LED" / `kPrePitchActive` | Pitch LED | build.pre | toggle | |
| pre.comp.on | §PRE Pedals LED / `kPreCompActive` | Comp LED | build.pre | toggle | |
| pre.nam1.on | §PRE Pedals LED / `kPreNam1Active` | NAM 1 LED | build.pre | toggle | |
| pre.nam2.on | §PRE Pedals LED / `kPreNam2Active` | NAM 2 LED | build.pre | toggle | |
| pre.pitch.mode.transpose | §Pitch "TRANSPOSE" / `kPrePitchMode` | TRANSPOSE mode | build.pre | select | |
| pre.pitch.mode.octaver | §Pitch "OCTAVER (the default)" / `kPrePitchMode` | OCTAVER mode | build.pre | select | |
| pre.pitch.semi | §Pitch "SEMI −12 to +7" / `kPrePitchSemitones` | SEMI knob (TRANSPOSE) | build.pre | drag | |
| pre.pitch.mix | §Pitch "MIX" / `kPrePitchMix` | MIX knob (TRANSPOSE) | build.pre | drag | |
| pre.pitch.level | §Pitch "LEVEL" / `kPrePitchLevel` | LEVEL knob (TRANSPOSE) | build.pre | drag | |
| pre.pitch.char.instant | §Pitch "INSTANT (default)" / `kPrePitchTransChar` | INSTANT sub-mode pill | build.pre | select | |
| pre.pitch.char.poly | §Pitch "POLY" / `kPrePitchTransChar` | POLY sub-mode pill | build.pre | select | |
| pre.pitch.octdown | §Pitch "OCT DN" / `kPrePitchOctDown` | OCT DN knob (OCTAVER) | build.pre | drag | |
| pre.pitch.octup | §Pitch "OCT UP" / `kPrePitchOctUp` | OCT UP knob (OCTAVER) | build.pre | drag | |
| pre.pitch.dry | §Pitch "DRY" / `kPrePitchDry` | DRY knob (OCTAVER) | build.pre | drag | |
| pre.pitch.oct_level | §Pitch "LEVEL sets the output" (OCTAVER row) / `kPrePitchLevel` | LEVEL knob (OCTAVER) | build.pre | drag | |
| pre.pitch.voicing.vintage | §Pitch "VINTAGE" / `kPrePitchVoicing` | VINTAGE sub-mode pill | build.pre | select | |
| pre.pitch.voicing.modern | §Pitch "MODERN" / `kPrePitchVoicing` | MODERN sub-mode pill | build.pre | select | |
| pre.comp.input | §Compressor "INPUT" / `kPreCompAmount` | INPUT knob | build.pre | drag | |
| pre.comp.attack | §Compressor "ATTACK" / `kPreCompAttack` | ATTACK knob | build.pre | drag | |
| pre.comp.release | §Compressor "RELEASE" / `kPreCompRelease` | RELEASE knob | build.pre | drag | |
| pre.comp.output | §Compressor "OUTPUT" / `kPreCompLevel` | OUTPUT knob | build.pre | drag | |
| pre.nam1.capture | §NAM Pedals "Click an empty card to pick a capture; on a filled card, click once to select it and again to change it" / `kCtrlTagVoLumPreCaptureMenu` | NAM 1 capture chooser opener | build.pre | tap | |
| pre.nam2.capture | §NAM Pedals / `kCtrlTagVoLumPreCaptureMenu` | NAM 2 capture chooser opener | build.pre | tap | |
| pre.nam1.gain | §NAM Pedals "GAIN" / `kPreNam1Gain` | NAM 1 GAIN | build.pre | drag | |
| pre.nam1.bass | §NAM Pedals "BASS" / `kPreNam1Bass` | NAM 1 BASS | build.pre | drag | |
| pre.nam1.mid | §NAM Pedals "MID" / `kPreNam1Mid` | NAM 1 MID | build.pre | drag | |
| pre.nam1.midhz | §NAM Pedals "MID Hz" / `kPreNam1MidFreq` | NAM 1 MID Hz | build.pre | drag | |
| pre.nam1.treble | §NAM Pedals "TREBLE" / `kPreNam1Treble` | NAM 1 TREBLE | build.pre | drag | |
| pre.nam1.level | §NAM Pedals "LEVEL" / `kPreNam1Level` | NAM 1 LEVEL | build.pre | drag | |
| pre.nam2.gain | §NAM Pedals / `kPreNam2Gain` | NAM 2 GAIN | build.pre | drag | |
| pre.nam2.bass | `kPreNam2Bass` | NAM 2 BASS | build.pre | drag | |
| pre.nam2.mid | `kPreNam2Mid` | NAM 2 MID | build.pre | drag | |
| pre.nam2.midhz | `kPreNam2MidFreq` | NAM 2 MID Hz | build.pre | drag | |
| pre.nam2.treble | `kPreNam2Treble` | NAM 2 TREBLE | build.pre | drag | |
| pre.nam2.level | `kPreNam2Level` | NAM 2 LEVEL | build.pre | drag | |
| pre.capture.select | §NAM Pedals capture menu / `kPreNam1Capture`, `kPreNam2Capture` | capture row | pre.capture_menu | tap | |
| pre.capture.select_custom | §NAM Pedals "Your own pedals appear under CUSTOM" / `kPreNam1Capture` custom index (64 and up) | custom pedal row loads that capture | pre.capture_menu | tap | |
| pre.capture.empty | menu row 0 EMPTY / `VoLumSceneRig.inc.cpp` | empty the slot | pre.capture_menu | tap | |
| pre.capture.custom | §NAM Pedals "Your own pedals appear under CUSTOM" | CUSTOM group | pre.capture_menu | view | |
| pre.capture.manage | §Custom Pedals "Manage custom pedals..." | opens custom pedal management | pre.capture_menu | tap | |
| pre.capture.close | §Keyboard Shortcuts `Esc` | close the capture chooser | pre.capture_menu | tap | |
| pre.comp.ratio | `kPreCompRatio` | not surfaced | - | - | the desktop knob row has no RATIO; the param keeps its default |
| pre.comp.mix | `kPreCompMix` | not surfaced | - | - | the desktop knob row has no MIX; the param keeps its default |
| pre.boost | `kBoostActive`, `kBoostDrive`, `kBoostTone`, `kBoostLevel` | not surfaced | - | - | reserved legacy params kept only for old chunks; PRE captures replaced the block |

## BUILD — POST effects

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| post.lock | §Each Amp Remembers Its Rig "lock in the ... POST header" | POST lock | build.post | toggle | |
| post.store | §Each Amp Remembers Its Rig "Store arrow" | Store arrow when locked effects differ | build.post | tap | |
| post.card.chorus | §POST Effects "click an effect card" / `kCtrlTagVoLumChorusCard` | Chorus card | build.post | select | |
| post.card.delay | §POST Effects / `kCtrlTagVoLumDelayCard` | Delay card | build.post | select | |
| post.card.reverb | §POST Effects / `kCtrlTagVoLumReverbCard` | Reverb card | build.post | select | |
| post.card.trem | §POST Effects / `kCtrlTagVoLumTremoloCard` | Tremolo card | build.post | select | |
| post.chorus.on | §POST Effects LED / `kChorusActive` | Chorus LED | build.post | toggle | |
| post.delay.on | §POST Effects LED / `kDelayActive` | Delay LED | build.post | toggle | |
| post.reverb.on | §POST Effects LED / `kReverbActive` | Reverb LED | build.post | toggle | |
| post.trem.on | §POST Effects LED / `kTremoloActive` | Tremolo LED | build.post | toggle | |
| post.mode_memory | §POST Effects "Each mode remembers its own knob settings" | same | - | engine | |
| post.chorus.mode.classic | §Chorus "CLASSIC" / `kChorusMode` | CLASSIC | build.post | select | |
| post.chorus.mode.warped | §Chorus "WARPED" | WARPED | build.post | select | |
| post.chorus.mode.clear | §Chorus "CLEAR" | CLEAR | build.post | select | |
| post.chorus.mode.ensemble | §Chorus "ENSEMBLE (default)" | ENSEMBLE | build.post | select | |
| post.chorus.rate | §Chorus "RATE" / `kChorusRate` | RATE | build.post | drag | |
| post.chorus.depth | §Chorus "DEPTH" / `kChorusDepth` | DEPTH | build.post | drag | |
| post.chorus.tone | §Chorus "TONE" / `kChorusTone` | TONE | build.post | drag | |
| post.chorus.width | §Chorus "WIDTH (0% is mono)" / `kChorusWidth` | WIDTH | build.post | drag | |
| post.chorus.mix | §Chorus "MIX" / `kChorusMix` | MIX | build.post | drag | |
| post.delay.mode.digital | §Delay "DIGITAL" / `kDelayMode` | DIGITAL | build.post | select | |
| post.delay.mode.analog | §Delay "ANALOG" | ANALOG | build.post | select | |
| post.delay.mode.reverse | §Delay "REVERSE" | REVERSE | build.post | select | |
| post.delay.time | §Delay "TIME" / `kDelayTime` | TIME | build.post | drag | |
| post.delay.feedback | §Delay "FEEDBACK" / `kDelayFeedback` | FEEDBACK | build.post | drag | |
| post.delay.mix | §Delay "MIX" / `kDelayMix` | MIX | build.post | drag | |
| post.delay.tone | §Delay "TONE" / `kDelayTone` | TONE | build.post | drag | |
| post.delay.grit | §Delay "GRIT" (DIGITAL) / `kDelayAge` | GRIT | build.post | drag | |
| post.delay.wear | §Delay "WEAR" (ANALOG) / `kDelayAge` | WEAR | build.post | drag | |
| post.delay.bloom | §Delay "BLOOM" (REVERSE) / `kDelayAge` | BLOOM | build.post | drag | |
| post.delay.pingpong | §Delay "PING-PONG (Digital and Analog)" / `kDelayPingPong` | PING-PONG switch | build.post | toggle | |
| post.delay.sync | §Delay "TEMPO SYNC" / `kDelaySync` | TEMPO SYNC switch | build.post | toggle | |
| post.delay.division | §Delay "DIVISION: 1/2 ... 1/16" / `kDelayDivision` | DIVISION stepper in TIME's place | build.post | select | |
| post.reverb.mode.hall | §Reverb "HALL" / `kReverbMode` | HALL | build.post | select | |
| post.reverb.mode.plate | §Reverb "PLATE" | PLATE | build.post | select | |
| post.reverb.mode.oktaverb | §Reverb "OKTAVERB" | OKTAVERB | build.post | select | |
| post.reverb.mix | §Reverb "MIX" / `kReverbMix` | MIX | build.post | drag | |
| post.reverb.decay | §Reverb "DECAY" / `kReverbDecay` | DECAY | build.post | drag | |
| post.reverb.tone | §Reverb "TONE" / `kReverbTone` | TONE | build.post | drag | |
| post.reverb.predelay | §Reverb "PRE-DLY" / `kReverbPreDelay` | PRE-DLY | build.post | drag | |
| post.reverb.sub.halo | §Reverb "HALO" / `kReverbSubMode` | HALO pill (OKTAVERB) | build.post | select | |
| post.reverb.sub.shimmer | §Reverb "SHIMMER" / `kReverbSubMode` | SHIMMER pill (OKTAVERB) | build.post | select | |
| post.reverb.sub.bloom | §Reverb "BLOOM" / `kReverbSubMode` | BLOOM pill (OKTAVERB) | build.post | select | |
| post.reverb.intensity | §Reverb "INTENSITY" / `kReverbShimmer` | INTENSITY knob (OKTAVERB) | build.post | drag | |
| post.trem.mode.optical | §Tremolo "OPTICAL (default)" / `kTremoloMode` | OPTICAL | build.post | select | |
| post.trem.mode.bias | §Tremolo "BIAS" | BIAS | build.post | select | |
| post.trem.mode.harmonic | §Tremolo "HARMONIC" | HARMONIC | build.post | select | |
| post.trem.rate | §Tremolo "RATE" / `kTremoloRate` | RATE | build.post | drag | |
| post.trem.depth | §Tremolo "DEPTH" / `kTremoloDepth` | DEPTH | build.post | drag | |
| post.trem.shape | §Tremolo "SHAPE (smooth to square)" / `kTremoloShape` | SHAPE | build.post | drag | |
| post.trem.mix | §Tremolo "MIX" / `kTremoloMix` | MIX | build.post | drag | |
| post.trem.xover | §Tremolo "X-OVER" (HARMONIC) / `kTremoloCrossover` | X-OVER knob | build.post | drag | |
| post.trem.sync | §Tremolo "TEMPO SYNC" / `kTremoloSync` | TEMPO SYNC switch | build.post | toggle | |
| post.trem.division | §Tremolo / `kTremoloDivision` | DIVISION stepper in RATE's place | build.post | select | |
| post.tempo | §Tempo "follow the metronome BPM, even with the click off" | Delay/Tremolo sync follows `metronome.bpm` | - | engine | |
| post.tempo.daw | §Tempo "In a DAW they follow the song" | no host transport | - | - | Android VoLum is a standalone app; there is no DAW song tempo to follow |

## Presets

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| presets.menu.default | §The Preset Menu "Default (factory settings)" | Default row resets the amp | presets.menu | tap | |
| presets.menu.factory | §The Preset Menu "FACTORY" / §Factory Presets | Factory preset rows | presets.menu | tap | |
| presets.menu.user | §The Preset Menu "USER" | User preset rows | presets.menu | tap | |
| presets.menu.overwrite | §The Preset Menu "Overwrite "name" ... after asking" | Overwrite row asks to confirm | presets.menu | tap | |
| presets.menu.save_new | §The Preset Menu "Save current as new..." | opens the name box | presets.menu | tap | |
| presets.menu.manage | §The Preset Menu "Manage presets..." | opens preset management | presets.menu | tap | |
| presets.menu.close | §Keyboard Shortcuts `Esc` | close the menu | presets.menu | tap | |
| presets.save.open | §Save A Preset "Press Ctrl+S" | save control in BUILD opens the name box | build.amp | tap | |
| presets.factory_catalog | §Factory Presets table (21 presets) | same catalogue in the fake library | - | engine | |
| presets.save.dedupe | §Save A Preset "If a name is taken twice, VoLum adds a number" | same naming rule | - | engine | |
| presets.manage.overwrite | §The Preset Menu "Manage presets... overwrite" / `kRowOverwriteBase` | overwrite on a row asks to confirm | presets.manage | tap | |
| presets.manage.rename | §The Preset Menu "rename" / `kRowRenameBase` | rename on a row opens the name box | presets.manage | tap | |
| presets.manage.delete | §The Preset Menu "delete" / `kRowDeleteBase` | delete on a row asks to confirm | presets.manage | tap | |
| presets.manage.close | `kClose` | close management | presets.manage | tap | |
| name.field | §Save A Preset "the name box" / `kCtrlTagVoLumNameDialog` | name text field | dialog.name | text | |
| name.save | §Save A Preset "Save creates a new User preset" | Save button | dialog.name | tap | |
| name.update | §Save A Preset "On one of your User presets, the button reads Update" | Update button on a User preset | dialog.name | tap | |
| name.cancel | §Save A Preset "Esc or Cancel" | Cancel | dialog.name | tap | |
| name.rename | §The Preset Menu "rename" through the shared name dialog | confirm a rename | dialog.name | tap | |

## Tuner and metronome

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| tuner.note | §Tuner And Metronome / `VoLumTuner` | detected note (fake engine pitch) | tuner | view | |
| tuner.cents | §Tuner And Metronome / `VoLumTuner` | cents deviation needle/strobe | tuner | view | |
| tuner.muted | §Tuner And Metronome "Your guitar is muted while it is open" | muted notice | tuner | view | |
| tuner.close | §Tuner And Metronome "Close it with Esc or a click outside" | close control (and tap outside) | tuner | tap | |
| metronome.on | §Tuner And Metronome "Switch it on" | on/off switch | metronome | toggle | |
| metronome.bpm | §Tuner And Metronome "tempo (30 to 300 BPM)" | BPM control | metronome | drag | |
| metronome.volume | §Tuner And Metronome "volume" | volume control | metronome | drag | |
| metronome.sig.1_4 | §Tuner And Metronome "1/4" | 1/4 | metronome | select | |
| metronome.sig.2_4 | §Tuner And Metronome "2/4" | 2/4 | metronome | select | |
| metronome.sig.3_4 | §Tuner And Metronome "3/4" | 3/4 | metronome | select | |
| metronome.sig.4_4 | §Tuner And Metronome "4/4" | 4/4 | metronome | select | |
| metronome.sig.6_8 | §Tuner And Metronome "6/8" | 6/8 | metronome | select | |
| metronome.beat | `kCtrlTagVoLumMetronome` beat display | beat indicator (fake engine clock) | metronome | view | |
| metronome.close | §Tuner And Metronome | close control | metronome | tap | |

## MIDI foot controllers

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| midi.channel.all | §The MIDI Tab "All channels (default)" | All channels | settings.midi | select | |
| midi.channel.one | §The MIDI Tab "One channel" | One channel | settings.midi | select | |
| midi.channel.number | §The MIDI Tab "CH 1 to CH 16" | channel stepper | settings.midi | select | |
| midi.recall_cc | §The MIDI Tab "Recall CC (0 to 119, default 102)" | Recall CC stepper | settings.midi | tap | |
| midi.bank.next | §The MIDI Tab "16 banks of 8 switches ... arrows" | next bank | settings.midi | tap | |
| midi.bank.prev | §The MIDI Tab "arrows" | previous bank | settings.midi | tap | |
| midi.slot.assign | §The MIDI Tab "Click a switch to choose its Sound" | tap a switch opens the picker for that program | settings.midi | tap | |
| midi.slot.move | §The MIDI Tab "drag it to move it" | drag; also the switch's "Move down" accessibility action | settings.midi | action:Move down | |
| midi.slot.clear | §The MIDI Tab "hover and click × to clear it" | clear control on a switch (no hover on touch) | settings.midi | tap | |
| midi.device.select | §Connect The Controller "pick the controller's MIDI input" | Android MIDI input device (USB or BLE) | settings.audio | tap | |
| midi.device.ble_scan | §Connect The Controller | scan for Bluetooth LE MIDI controllers (fake engine finds one) | settings.audio | tap | |
| midi.device.lost | §MIDI (controller unplugged) | MIDI device disconnected notice | settings.audio | view | |
| midi.program_change | §MIDI Foot Controllers "Program Change N recalls the Sound on program N" | fake engine MIDI events recall Sounds | - | engine | |
| midi.recall_cc_message | §MIDI Foot Controllers "CC 102 with value N does the same" | same | - | engine | |
| midi.empty_program | §MIDI Foot Controllers "A program number with no Sound is ignored" | same | - | engine | |
| midi.channel_filter | §The MIDI Tab "One channel ... lets two VoLums share one MIDI cable" | same | - | engine | |
| midi.learn | §The MIDI Tab "does not use ... MIDI Learn" | program slots are the assign model | - | - | desktop VoLum deliberately has no MIDI Learn; the slot grid (midi.slot.*) is the learn/assign model |
| midi.bank_select | §The MIDI Tab "does not switch banks with Bank Select" | none | - | - | desktop ignores Bank Select, notes, pitch bend and MIDI output on purpose |
| midi.vst3_channel | §The MIDI Tab "VST3: the plugin receives MIDI on channel 1 only" | none | - | - | plug-in format rule; Android VoLum is a standalone app |

## Your own amps, IRs and pedals

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| custom.amp.add | §Custom Amps 1 "Click + in the CUSTOM part" | + in CUSTOM opens the builder | amp.picker | tap | |
| custom.amp.edit | §Custom Amps "pen ... icons" | pen on a custom amp opens the builder | amp.picker | tap | |
| custom.amp.delete | §Custom Amps "bin icons" | bin on a custom amp asks to confirm | amp.picker | tap | |
| builder.name | §Custom Amps 2 "Name the amp" / `kEditName` | amp name field | custom.amp_builder | text | |
| builder.add_files | §Custom Amps 2 "+ Add .nam files" / `kAddFile` | Storage Access Framework multi-pick of .nam files | custom.amp_builder | file | |
| builder.file.cab | §Custom Amps 3 "Give each file a cab slot" / `kFileSpeakerBase` | cab slot per file | custom.amp_builder | select | |
| builder.file.channel | §Custom Amps 3 "and a channel" / `kFileChannelBase` | channel per file | custom.amp_builder | select | |
| builder.file.remove | `kFileRemoveBase` | remove a file | custom.amp_builder | tap | |
| builder.cab_name | `kCabNameBase` cab-slot rename chips | rename a cab slot | custom.amp_builder | text | |
| builder.art | `kArtBase` art swatches | pick the custom art style | custom.amp_builder | select | |
| builder.save | §Custom Amps 4 "Click Save amp" / `kBuilderSave` | Save amp | custom.amp_builder | tap | |
| builder.close | `kClose` | close without saving | custom.amp_builder | tap | |
| builder.autofill | §Custom Amps 3 "Files named like V30-MyAmp-2.nam fill this in" | same parser | - | engine | |
| ir.menu.select | §Custom IRs 3 "Pick the IR from the Custom IR menu" | IR row | cab.ir_menu | tap | |
| ir.menu.manage | §Custom IRs 2 "Manage custom IRs..." | opens IR management | cab.ir_menu | tap | |
| ir.menu.close | §Keyboard Shortcuts `Esc` | close the IR menu | cab.ir_menu | tap | |
| ir.import | §Custom IRs 2 "+ Import IR (.wav)", "pick several files at once" | Storage Access Framework multi-pick of .wav | custom.ir_manage | file | |
| ir.import.invalid | §Custom IRs "checks that an import is a readable WAV" | rejected pick shows the error banner, nothing copied | custom.ir_manage | file | |
| ir.config.open | §Custom IRs "click the gear on its row" / `kRowIrCfgBase` | gear on an IR row | custom.ir_manage | tap | |
| ir.config.level | §Custom IRs "Level" / `kIrCfgTrimDown`/`Up` | IR level | custom.ir_manage | drag | |
| ir.config.lowcut | §Custom IRs "Low cut" / `kIrCfgLowDown`/`Up` | IR low cut | custom.ir_manage | drag | |
| ir.config.highcut | §Custom IRs "High cut" / `kIrCfgHighDown`/`Up` | IR high cut | custom.ir_manage | drag | |
| ir.rename | `kRowRenameBase` | rename an IR | custom.ir_manage | tap | |
| ir.delete | §Deleting Content / `kRowDeleteBase` | delete an IR (asks first) | custom.ir_manage | tap | |
| ir.manage.close | `kClose` | close IR management | custom.ir_manage | tap | |
| ir.naming | §Custom IRs "Long file names are shortened in the middle ... a name that is already taken gets a number" | same rules | - | engine | |
| pedal.import | §Custom Pedals "+ Import pedal (.nam)" | Storage Access Framework multi-pick of .nam | custom.pedal_manage | file | |
| pedal.rename | `kRowRenameBase` | rename a pedal | custom.pedal_manage | tap | |
| pedal.delete | §Deleting Content / `kRowDeleteBase` | delete a pedal (asks first) | custom.pedal_manage | tap | |
| pedal.manage.close | `kClose` | close pedal management | custom.pedal_manage | tap | |
| content.delete_fallback | §Deleting Content "a deleted amp falls back ... a deleted IR returns to a stock cab" | same fallbacks on MAIN and SUPPORT; deleting the custom SUPPORT amp switches Dual Amp off | - | engine | |

## Packs

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| pack.export.open | §Back Up And Share "Export Pack..." | opens export | settings.system | tap | |
| pack.import.open | §Back Up And Share "Import Pack..." | Storage Access Framework open of a .volumpack, then the preview | settings.system | file | |
| pack.export.scope.everything | §Export "Everything" | Everything scope | pack.export | select | |
| pack.export.scope.sounds | §Export "Sounds: the presets you tick" | Sounds scope | pack.export | select | |
| pack.export.scope.amp | §Export "A whole amp" | whole-amp scope | pack.export | select | |
| pack.export.sound_tick | §Export "the presets you tick" | tick a Sound | pack.export | toggle | |
| pack.export.amp_pick | §Export "a custom amp with all its presets" | pick the custom amp | pack.export | select | |
| pack.export.also | §Export "ALSO INCLUDING lists what your choice needs" | ALSO INCLUDING band | pack.export | view | |
| pack.export.also.support | §Export "ALSO INCLUDING ... like an IR or a pedal" / `VoLumPack.h` closure over `activeIrId`, `supportActiveIrId`, `supportCustomId` | a Dual Amp preset also brings its SUPPORT custom IR and custom SUPPORT amp | - | engine | |
| pack.export.save | §Export / `VOLUM_PACK_SAVE_PATH` path | Storage Access Framework create-document | pack.export | file | |
| pack.export.close | `kClose` | close export | pack.export | tap | |
| pack.import.rows | §Import "The preview lists everything in the Pack" | preview rows | pack.import | view | |
| pack.import.tick | §Import "Untick what you do not want" | tick/untick a row | pack.import | toggle | |
| pack.import.outcome | §Import "Add, Replace, Keep mine, Reloads (playing now) or Skip" | per-row outcome label (conflict fixture) | pack.import | view | |
| pack.import.merge.overwrite | §Import "Overwrite: the Pack's version wins" | Overwrite | pack.import | select | |
| pack.import.merge.add | §Import "Add: your version wins" | Add | pack.import | select | |
| pack.import.merge.reset | §Import "Reset ... Only available while every item is ticked" | Reset | pack.import | select | |
| pack.import.machine | §Import "Also restore machine settings, which starts unticked" | Also restore machine settings | pack.import | toggle | |
| pack.import.confirm | §Import | Import button | pack.import | tap | |
| pack.import.close | `kClose` | close the preview | pack.import | tap | |
| pack.import.refused | §Import "a Pack that needs more slots than are left is refused before anything changes" | refusal notice | pack.import | view | |
| pack.import.damaged | §Import "A damaged Pack changes nothing" | damaged-Pack notice | pack.import | view | |
| pack.packbak | §Import "keeps your previous library as volum-content.json.packbak" | same backup inside app storage | - | engine | |
| pack.plugin_rules | §Import "A plug-in has no such box" | none | - | - | plug-in-only rule; Android VoLum is a standalone app |

## Settings

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| settings.tab.signal | §Settings SIGNAL / `kTabSignal` | SIGNAL tab | settings.midi | select | |
| settings.tab.midi | §Settings MIDI / `kTabMidi` | MIDI tab | settings.signal | select | |
| settings.tab.system | §Settings SYSTEM / `kTabSystem` | SYSTEM tab | settings.signal | select | |
| settings.close | §Settings "close it the same way, with Esc, or by clicking outside" | close control | settings.signal | tap | |
| signal.calibration.level | §SIGNAL Tab "enter your interface's input level in dBu" / `kInputCalibrationLevel` | dBu field | settings.signal | text | |
| signal.calibration.on | §SIGNAL Tab "Calibrate input" / `kCalibrateInput` | Calibrate input switch | settings.signal | toggle | |
| signal.calibration.unavailable | §SIGNAL Tab "This model has no capture level" | notice for captures without a level | settings.signal | view | |
| signal.output.raw | §SIGNAL Tab "Raw" / `kOutputMode` | Raw | settings.signal | select | |
| signal.output.normalized | §SIGNAL Tab "Normalized (default)" | Normalized | settings.signal | select | |
| signal.output.calibrated | §SIGNAL Tab "Calibrated needs a capture that stores its output level" | Calibrated (disabled for bundled amps) | settings.signal | select | |
| signal.perf.full | §SIGNAL Tab "FULL (default)" | FULL | settings.signal | select | |
| signal.perf.lite | §SIGNAL Tab "LITE" | LITE | settings.signal | select | |
| signal.animate_art | §SIGNAL Tab "Animate art in PLAY" | Animate art switch | settings.signal | toggle | |
| signal.audio_devices | §SIGNAL Tab "Audio & MIDI devices..." | opens the Android audio & MIDI device panel | settings.signal | tap | |
| system.shortcuts | §SYSTEM Tab "Keyboard shortcuts" | none | - | - | no keyboard on a phone stage rig; every shortcut's action has its own touch feature id |
| system.model_info | §SYSTEM Tab "Model information" | model information card | settings.system | view | |
| system.about.version | §SYSTEM Tab "About: version" | version | settings.system | view | |
| system.about.manual | §SYSTEM Tab "Read the manual" | opens the manual link (recorded by the fake host) | settings.system | tap | |
| system.update.auto | §Update Checks "Check for updates automatically" | switch | settings.system | toggle | |
| system.update.check_now | `AboutControl` "Check now" | check now (fake engine answers) | settings.system | tap | |
| system.update.badge | §Update Checks "A gold dot on the gear" / `kCtrlTagVoLumUpdateBadge` | gold dot on the gear | play | view | |
| system.update.link | §Update Checks "Settings links to the release page" | release-page link | settings.system | tap | |
| system.share_log | §Report A Bug "Attach volum.log" | share the diagnostic log through the Android share sheet (the file has no user-visible path) | settings.system | tap | |
| audio.input.device | §Audio & MIDI Devices "input ... devices" | input device list, USB interface first | settings.audio | select | |
| audio.input.channel | §Audio & MIDI Devices "Use one mono input for the guitar" | interface input channel | settings.audio | select | |
| audio.output.device | §Audio & MIDI Devices "output devices" | output device list | settings.audio | select | |
| audio.sample_rate | §Audio & MIDI Devices "the sample rate" | sample rate | settings.audio | select | |
| audio.buffer | §Audio & MIDI Devices "the buffer size ... smaller buffer means less delay but more CPU" | buffer / latency mode | settings.audio | select | |
| audio.devices.close | §Audio & MIDI Devices | close the device panel | settings.audio | tap | |
| audio.driver | §Audio & MIDI Devices "Choose the driver" | none | - | - | Android has one low-latency path (AAudio); there is no driver to choose |
| audio.permission.grant | microphone permission (Android only) | denied state offers "Allow microphone"; the fake engine grants on retry | audio.status | tap | |
| audio.permission.settings | microphone permission permanently denied | opens the app's system settings (recorded by the fake host) | audio.status | tap | |
| audio.permission.denied | microphone permission | denied notice | audio.status | view | |
| audio.no_interface | §Troubleshooting "Interface unplugged at start" | no-USB-interface notice | audio.status | view | |
| audio.no_interface.rescan | §Troubleshooting "Connect the interface" | rescan (fake engine finds the interface) | audio.status | tap | |
| audio.no_interface.builtin | §Troubleshooting | continue with the phone's built-in input | audio.status | tap | |
| audio.unplugged | §Troubleshooting "Interface unplugged" (mid-session) | interface-lost notice, audio stopped | audio.status | view | |
| audio.unplugged.reconnect | §Troubleshooting "Connect the interface and restart" | reconnect without restarting the app (fake engine) | audio.status | tap | |
| audio.status.close | | close the audio status panel | audio.status | tap | |
| audio.input.indicator | §Quick Start 1 | input state (device, permission) readable at a glance | play | view | |

## Not applicable on Android

| id | desktop source | Android mapping | screen | interaction | N/A reason |
| --- | --- | --- | --- | --- | --- |
| keys.everywhere | §Keyboard Shortcuts "Everywhere" (P, T, M, H, Ctrl+S, Esc) | none | - | - | no hardware keyboard on a phone rig; mapped to nav.view.*, nav.tuner.open, nav.metronome.open, nav.settings.open, presets.save.open, play.save and per-panel close controls |
| keys.build | §Keyboard Shortcuts "BUILD" (1/2/3, arrows, Tab, S, Enter, Space, B) | none | - | - | keyboard only; mapped to nav.section.*, amp.list.select, amp.channel, pre.card.*/post.card.*, dual.focus.*, amp.cab.*, *.on |
| keys.knob | §Keyboard Shortcuts "Selected knob" | none | - | - | keyboard only; mapped to knob drag, amp.knob.exact and amp.knob.reset |
| keys.play | §Keyboard Shortcuts "PLAY" (arrows, 1-8) | none | - | - | keyboard only; mapped to play.sound.next/prev and play.stomp.* |
| daw.plugin | §Using VoLum In A DAW (VST3, AU, Logic, GarageBand) | none | - | - | Android VoLum is a standalone app; there is no plug-in host |
| daw.initial_state | §Using VoLum In A DAW "A new plug-in starts with the amp settings from your standalone app" | none | - | - | no plug-in instances on Android |
| daw.shared_library | §Using VoLum In A DAW "custom library ... shared by every VoLum on the computer" | none | - | - | one app per device; nothing to share with |
| daw.b_key | §Using VoLum In A DAW "B switches the selected pedal" | none | - | - | keyboard and DAW transport rule |
| daw.latency | §Using VoLum In A DAW "VoLum reports its delay to the DAW" | none | - | - | no host to compensate; audio.buffer covers latency |
| trouble.another_copy | §Troubleshooting "VoLum says another copy is already running" | none | - | - | Android runs one task per app; a second copy cannot start |
| trouble.directsound_48k | §Troubleshooting "Windows: no 48 kHz ... DirectSound" | none | - | - | Windows driver issue; audio.sample_rate lists what the device offers |
| trouble.macos_au | §Troubleshooting "macOS: an AU from an older VoLum" | none | - | - | macOS AU only |
| trouble.macos_zip | §Troubleshooting "macOS: the plug-in does not show up after installing from a zip" | none | - | - | macOS install path only |
| files.locations | §Where VoLum Keeps Your Files | none | - | - | Android keeps settings, library and log in app-private storage with no user path; Packs (pack.*) and system.share_log replace file access |

## Desktop parameters → feature ids

Every `EParams` entry (`NeuralAmpModeler/VoLumParams.h`):

| param | feature id |
| --- | --- |
| kInputLevel, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel | amp.knob.input, amp.knob.gate, amp.knob.bass, amp.knob.mid, amp.knob.treble, amp.knob.output |
| kNoiseGateActive, kEQActive | amp.gate.toggle, amp.eq.toggle |
| kIRToggle | amp.cab.custom_ir, ir.menu.select |
| kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayMode, kDelayTone, kDelayAge, kDelayPingPong, kDelaySync, kDelayDivision | post.delay.on, post.delay.time, post.delay.feedback, post.delay.mix, post.delay.mode.*, post.delay.tone, post.delay.grit/wear/bloom, post.delay.pingpong, post.delay.sync, post.delay.division |
| kReverbActive, kReverbMix, kReverbDecay, kReverbTone, kReverbPreDelay, kReverbShimmer, kReverbMode, kReverbSubMode | post.reverb.on, post.reverb.mix, post.reverb.decay, post.reverb.tone, post.reverb.predelay, post.reverb.intensity, post.reverb.mode.*, post.reverb.sub.* |
| kBoostActive, kBoostDrive, kBoostTone, kBoostLevel | pre.boost (N/A) |
| kPreCompActive, kPreCompAmount, kPreCompRatio, kPreCompAttack, kPreCompRelease, kPreCompMix, kPreCompLevel | pre.comp.on, pre.comp.input, pre.comp.ratio (N/A), pre.comp.attack, pre.comp.release, pre.comp.mix (N/A), pre.comp.output |
| kPreNam1Active … kPreNam1Level | pre.nam1.on, pre.capture.select/select_custom, pre.nam1.gain/bass/mid/midhz/treble/level |
| kPreNam2Active … kPreNam2Level | pre.nam2.on, pre.capture.select/select_custom, pre.nam2.gain/bass/mid/midhz/treble/level |
| kCalibrateInput, kInputCalibrationLevel, kOutputMode | signal.calibration.on, signal.calibration.level, signal.output.* |
| kVoLumAmpeteRig | amp.channel, amp.cab.* (Ampete One maps channel + cab to its rig file) |
| kDualAmpActive, kDualAmpRoute, kMainAmpPan | dual.toggle, dual.route (N/A), dual.pan.main |
| kSupportAmpIdx, kSupportSpeakerIdx, kSupportChannelIdx | dual.support.select/select_custom/none, dual.support.cab, dual.support.channel |
| kSupportInputLevel … kSupportOutputLevel | dual.support.knob.* |
| kSupportNoiseGateActive, kSupportEQActive, kSupportAmpPan | dual.support.gate.toggle, dual.support.eq.toggle, dual.pan.support |
| kSupportIRToggle | dual.support.cab.custom_ir, dual.support.ir.select, dual.support.cab.custom_ir_disabled |
| kPrePitchActive, kPrePitchMode, kPrePitchSemitones, kPrePitchMix, kPrePitchOctDown, kPrePitchOctUp, kPrePitchDry, kPrePitchVoicing, kPrePitchLevel, kPrePitchTransChar | pre.pitch.on, pre.pitch.mode.*, pre.pitch.semi, pre.pitch.mix, pre.pitch.octdown, pre.pitch.octup, pre.pitch.dry, pre.pitch.voicing.*, pre.pitch.level / pre.pitch.oct_level, pre.pitch.char.* |
| kTremoloActive, kTremoloMode, kTremoloRate, kTremoloDepth, kTremoloShape, kTremoloMix, kTremoloCrossover, kTremoloSync, kTremoloDivision | post.trem.on, post.trem.mode.*, post.trem.rate, post.trem.depth, post.trem.shape, post.trem.mix, post.trem.xover, post.trem.sync, post.trem.division |
| kChorusActive, kChorusMode, kChorusRate, kChorusDepth, kChorusTone, kChorusWidth, kChorusMix | post.chorus.on, post.chorus.mode.*, post.chorus.rate, post.chorus.depth, post.chorus.tone, post.chorus.width, post.chorus.mix |

## User-guide heading checklist

Every `##` / `###` heading of `docs/user-guide.en.md` (`FeaturesMarkdownTest`
fails when a heading is missing here or a listed id is unknown):

| heading | feature ids |
| --- | --- |
| ## Contents | nav.view.build, nav.settings.open (navigation index only) |
| ## Quick Start | signal.audio_devices, audio.input.device, play.sound.recall, nav.view.build, tip.build, nav.section.pre, nav.section.post, presets.save.open, play.add_this |
| ## The BUILD Screen | amp.list.select, amp.hero, amp.channel, amp.knob.input, nav.section.pre, nav.section.amp, nav.section.post, header.preset_name, header.preset.prev, header.preset.next, amp.knob.reset |
| ### Each Amp Remembers Its Rig | amp.rig_memory, pre.lock, pre.store, post.lock, post.store, pre.lock.play_unlocks |
| ## Amps And Cabs | amp.list.select, amp.channel, amp.cab.none, amp.cab.g12, amp.cab.g65, amp.cab.v30, amp.cab.custom_ir, amp.knob.input, amp.knob.gate, amp.knob.bass, amp.knob.mid, amp.knob.treble, amp.knob.output, amp.lite_note |
| ## PRE Pedals | pre.card.pitch, pre.card.comp, pre.card.nam1, pre.card.nam2, pre.pitch.on, pre.comp.on, pre.nam1.on, pre.nam2.on |
| ### Pitch | pre.pitch.mode.transpose, pre.pitch.mode.octaver, pre.pitch.semi, pre.pitch.mix, pre.pitch.level, pre.pitch.char.instant, pre.pitch.char.poly, pre.pitch.octdown, pre.pitch.octup, pre.pitch.dry, pre.pitch.oct_level, pre.pitch.voicing.vintage, pre.pitch.voicing.modern |
| ### Compressor | pre.comp.input, pre.comp.attack, pre.comp.release, pre.comp.output |
| ### NAM Pedals | pre.nam1.capture, pre.nam2.capture, pre.capture.select, pre.capture.empty, pre.capture.custom, pre.capture.select_custom, pre.nam1.gain, pre.nam1.bass, pre.nam1.mid, pre.nam1.midhz, pre.nam1.treble, pre.nam1.level |
| ## POST Effects | post.card.chorus, post.card.delay, post.card.reverb, post.card.trem, post.chorus.on, post.delay.on, post.reverb.on, post.trem.on, post.mode_memory |
| ### Chorus | post.chorus.mode.classic, post.chorus.mode.warped, post.chorus.mode.clear, post.chorus.mode.ensemble, post.chorus.rate, post.chorus.depth, post.chorus.tone, post.chorus.width, post.chorus.mix |
| ### Delay | post.delay.mode.digital, post.delay.mode.analog, post.delay.mode.reverse, post.delay.time, post.delay.feedback, post.delay.mix, post.delay.tone, post.delay.grit, post.delay.wear, post.delay.bloom, post.delay.pingpong, post.delay.sync, post.delay.division |
| ### Reverb | post.reverb.mode.hall, post.reverb.mode.plate, post.reverb.mode.oktaverb, post.reverb.mix, post.reverb.decay, post.reverb.tone, post.reverb.predelay, post.reverb.sub.halo, post.reverb.sub.shimmer, post.reverb.sub.bloom, post.reverb.intensity |
| ### Tremolo | post.trem.mode.optical, post.trem.mode.bias, post.trem.mode.harmonic, post.trem.rate, post.trem.depth, post.trem.shape, post.trem.mix, post.trem.xover, post.trem.sync, post.trem.division |
| ### Tempo | post.tempo, post.tempo.daw, metronome.bpm |
| ## Dual Amp | dual.toggle, dual.support.choose, dual.support.select, dual.support.select_custom, dual.support.none, dual.focus.main, dual.focus.support, dual.pan.main, dual.pan.support, dual.polarity, dual.support.channel, dual.support.cab, dual.support.cab.custom_ir, dual.support.ir.select, dual.support.cab.custom_ir_disabled, dual.support.knob.input |
| ## Presets | header.preset.menu, presets.menu.factory, presets.menu.user |
| ### Factory Presets | presets.factory_catalog, presets.menu.factory, play.picker.factory |
| ### Save A Preset | presets.save.open, name.field, name.save, name.update, name.cancel, presets.save.dedupe |
| ### The Preset Menu | header.preset.menu, presets.menu.default, presets.menu.factory, presets.menu.user, presets.menu.overwrite, presets.menu.save_new, presets.menu.manage, presets.manage.overwrite, presets.manage.rename, presets.manage.delete, name.rename, header.unsaved, play.sound.invalid, dialog.confirm.accept |
| ## PLAY | nav.view.play, nav.view.build, play.view_memory, play.sound.program |
| ### The Five Starting Sounds | play.starting_sounds, play.sound.recall |
| ### Play Your Sounds | play.sound.recall, play.sound.next, play.sound.prev, play.sound.live, play.stomp.pitch, play.stomp.comp, play.stomp.nam1, play.stomp.nam2, play.stomp.chorus, play.stomp.delay, play.stomp.reverb, play.stomp.trem, play.stomp.edit, play.save |
| ### Add, Replace And Arrange Sounds | play.add_this, play.add_this.unnamed, play.add_this.full, play.add_sound, play.picker.program, play.picker.factory, play.picker.user, play.sound.replace, play.sound.clear, play.sound.reorder |
| ### Moving Art | play.art, signal.animate_art |
| ## MIDI Foot Controllers | midi.program_change, midi.recall_cc_message, midi.empty_program |
| ### Connect The Controller | midi.device.select, midi.device.ble_scan, midi.device.lost |
| ### The MIDI Tab | midi.channel.all, midi.channel.one, midi.channel.number, midi.recall_cc, midi.vst3_channel, midi.bank.next, midi.bank.prev, midi.slot.assign, midi.slot.move, midi.slot.clear, midi.learn, midi.bank_select, midi.channel_filter |
| ## Tuner And Metronome | tuner.note, tuner.cents, tuner.muted, tuner.close, nav.tuner.open, nav.metronome.open, metronome.on, metronome.bpm, metronome.volume, metronome.sig.1_4, metronome.sig.2_4, metronome.sig.3_4, metronome.sig.4_4, metronome.sig.6_8, metronome.beat, metronome.close |
| ## Your Own Amps, IRs And Pedals | custom.amp.add, ir.import, pedal.import |
| ### Custom Amps | custom.amp.add, custom.amp.edit, custom.amp.delete, amp.list.select_custom, dual.support.select_custom, builder.name, builder.add_files, builder.file.cab, builder.file.channel, builder.file.remove, builder.cab_name, builder.art, builder.save, builder.close, builder.autofill |
| ### Custom IRs | amp.cab.custom_ir, amp.cab.custom_ir_disabled, dual.support.cab.custom_ir, dual.support.cab.custom_ir_disabled, dual.support.ir.select, ir.menu.select, ir.menu.manage, ir.import, ir.import.invalid, ir.config.open, ir.config.level, ir.config.lowcut, ir.config.highcut, ir.naming, ir.rename, ir.delete |
| ### Custom Pedals | pre.capture.manage, pedal.import, pedal.rename, pedal.delete, pre.capture.custom, pre.capture.select_custom |
| ### Deleting Content | content.delete_fallback, dialog.confirm.message, dialog.confirm.accept, dialog.confirm.cancel |
| ## Back Up And Share With Packs | pack.export.open, pack.import.open |
| ### Export | pack.export.scope.everything, pack.export.scope.sounds, pack.export.scope.amp, pack.export.sound_tick, pack.export.amp_pick, pack.export.also, pack.export.also.support, pack.export.save |
| ### Import | pack.import.rows, pack.import.tick, pack.import.outcome, pack.import.merge.overwrite, pack.import.merge.add, pack.import.merge.reset, pack.import.machine, pack.import.confirm, pack.import.refused, pack.import.damaged, pack.packbak, pack.plugin_rules |
| ## Settings | nav.settings.open, settings.close, settings.tab.signal, settings.tab.midi, settings.tab.system |
| ### SIGNAL Tab | signal.calibration.level, signal.calibration.on, signal.calibration.unavailable, signal.output.raw, signal.output.normalized, signal.output.calibrated, signal.perf.full, signal.perf.lite, signal.animate_art, signal.audio_devices |
| ### MIDI Tab | settings.tab.midi, midi.channel.all |
| ### SYSTEM Tab | settings.tab.system, system.shortcuts, system.model_info, pack.export.open, pack.import.open, system.about.version, system.about.manual |
| ### Update Checks And Privacy | system.update.badge, system.update.link, system.update.auto, system.update.check_now |
| ### Audio & MIDI Devices (Standalone) | signal.audio_devices, audio.driver, audio.input.device, audio.input.channel, audio.output.device, audio.sample_rate, audio.buffer, midi.device.select, audio.devices.close, audio.permission.grant, audio.permission.settings, audio.permission.denied |
| ## Using VoLum In A DAW | daw.plugin, daw.initial_state, daw.shared_library, daw.b_key, daw.latency |
| ## Keyboard Shortcuts | keys.everywhere, keys.build, keys.knob, keys.play, system.shortcuts |
| ## Troubleshooting | status.output_safety, tuner.muted, dual.polarity, midi.recall_cc, audio.no_interface, audio.no_interface.rescan, audio.no_interface.builtin, audio.unplugged, audio.unplugged.reconnect, trouble.another_copy, trouble.directsound_48k, trouble.macos_au, trouble.macos_zip, banner.error |
| ## Where VoLum Keeps Your Files | files.locations, system.share_log |
| ## Report A Bug Or Request A Feature | system.share_log, system.about.manual |
