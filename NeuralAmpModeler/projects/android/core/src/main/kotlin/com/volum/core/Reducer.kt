package com.volum.core

/** Pure state transitions. Requests that need the outside world go through [FakeEngine] first (see [VoLumStore]). */
object Reducer {
    fun reduce(s: VoLumState, a: Action): VoLumState = when (a) {
        is Action.SetView -> setView(s, a.view)
        is Action.SetSection -> s.ui(s.ui.copy(section = a.section, overlays = emptyList()))
        is Action.FocusPre -> s.ui(s.ui.copy(section = Section.PRE, preFocus = a.block))
        is Action.FocusPost -> s.ui(s.ui.copy(section = Section.POST, postFocus = a.block))
        is Action.FocusLane -> if (a.lane == Lane.SUPPORT && !s.dual) s else s.ui(s.ui.copy(lane = a.lane))
        is Action.Open -> open(s, a.overlay)
        is Action.Back -> back(s)
        is Action.CloseOverlay -> s.ui(s.ui.copy(overlays = s.ui.overlays.takeWhile { it != a.overlay }))

        is Action.SetParam -> setParam(s, a.param, a.value)
        is Action.ToggleParam -> setParam(s, a.param, if (s.on(a.param)) 0.0 else 1.0)
        is Action.ResetParam -> setParam(s, a.param, a.param.default)

        is Action.SelectAmp -> selectAmp(s, a.ampId)
        is Action.SelectChannel -> selectChannel(s, a.channel)
        is Action.SelectCab -> selectCab(s, a.cab)
        is Action.SelectIr -> if (s.supportFocused) selectSupportIr(s, a.irId)
        else if (s.library.ir(a.irId) == null) s
        else s.rig { it.copy(cab = Cab.CUSTOM_IR, irId = a.irId, dirty = true) }.pop<Overlay.IrMenu>()
        is Action.SelectSupportIr -> selectSupportIr(s, a.irId)
        is Action.SelectSupportAmp -> s.rig {
            val cab = if (it.support.cab == Cab.CUSTOM_IR) stockCab(s.library.amp(a.ampId), 0) else it.support.cab
            it.copy(support = it.support.copy(ampId = a.ampId, channel = 0, cab = cab, irId = null), dirty = true)
        }
            .let { if (a.ampId == null) it.ui(it.ui.copy(lane = Lane.MAIN)) else it }
            .pop<Overlay.SupportAmpMenu>()
        is Action.SelectSupportChannel -> {
            val amp = s.library.amp(s.rig.support.ampId)
            if (amp == null || a.channel !in 0 until amp.channels) s
            else s.rig {
                val leavesIr = it.support.cab == Cab.CUSTOM_IR && a.channel !in amp.noCabChannels
                val support = if (leavesIr) it.support.copy(channel = a.channel, cab = stockCab(amp, a.channel), irId = null) else it.support.copy(channel = a.channel)
                it.copy(support = support, dirty = true)
            }
        }
        is Action.SelectSupportCab -> {
            val amp = s.library.amp(s.rig.support.ampId)
            if (amp == null || (a.cab == Cab.CUSTOM_IR && (s.rig.support.channel !in amp.noCabChannels || s.rig.support.irId == null))) s
            else s.rig { it.copy(support = it.support.copy(cab = a.cab), dirty = true) }
        }
        is Action.SetLock -> setLock(s, a.section, a.locked)
        is Action.StoreLock -> storeLock(s, a.section)
        is Action.SelectCapture -> {
            val p = if (a.slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
            if (s.library.pedals.none { it.index == a.index }) s else setParam(s, p, a.index.toDouble()).pop<Overlay.CaptureMenu>()
        }

        is Action.StepPreset -> stepPreset(s, a.delta)
        is Action.LoadPreset -> loadPreset(s, a.presetId).pop<Overlay.PresetMenu>()
        is Action.ResetToDefault -> s.amp?.let { amp -> s.rig { Catalog.defaultRig(amp).copy(modeMemory = it.modeMemory) } }?.pop<Overlay.PresetMenu>() ?: s
        is Action.RequestOverwritePreset -> s.library.preset(a.presetId)?.takeIf { !it.factory }
            ?.let { s.ui(s.ui.copy(confirm = Confirm(ConfirmKind.OVERWRITE_PRESET, it.id, "Overwrite \"${it.name}\"?"))) } ?: s
        is Action.RequestDeletePreset -> s.library.preset(a.presetId)?.takeIf { !it.factory }
            ?.let { s.ui(s.ui.copy(confirm = Confirm(ConfirmKind.DELETE_PRESET, it.id, "Delete \"${it.name}\"?"))) } ?: s
        is Action.SetNameText -> (s.ui.top as? Overlay.NameDialog)?.let { s.replaceTop(it.copy(text = a.text)) } ?: s
        is Action.CommitName -> commitName(s)
        is Action.AcceptConfirm -> s.ui.confirm?.let { acceptConfirm(s.ui(s.ui.copy(confirm = null)), it) } ?: s
        is Action.CancelConfirm -> s.ui(s.ui.copy(confirm = null))
        is Action.DismissBanner -> s.ui(s.ui.copy(banner = null))

        is Action.RecallSound -> recall(s, a.program)
        is Action.StepSound -> stepSound(s, a.delta)
        is Action.AddThisSound -> addThisSound(s)
        is Action.SetPickerProgram -> (s.ui.top as? Overlay.SoundPicker)?.let { s.replaceTop(it.copy(program = a.program.coerceIn(0, 127))) } ?: s
        is Action.AssignSound -> s.library.preset(a.presetId)?.let { p ->
            s.lib(s.library.copy(sounds = s.library.sounds + (a.program.coerceIn(0, 127) to Sound(p.ampId, p.id))))
        }?.pop<Overlay.SoundPicker>() ?: s
        is Action.ClearSound -> s.lib(s.library.copy(sounds = s.library.sounds - a.program, live = s.library.live.takeIf { it != a.program }))
        is Action.MoveSound -> moveSound(s, a.program, a.delta)
        is Action.EditStomp -> editStomp(s, a.param)

        is Action.SetMetronomeOn -> s.copy(metronome = s.metronome.copy(on = a.on))
        is Action.SetBpm -> s.copy(metronome = s.metronome.copy(bpm = a.bpm.coerceIn(30.0, 300.0)))
        is Action.SetMetronomeVolume -> s.copy(metronome = s.metronome.copy(volume = a.volume.coerceIn(0.0, 1.0)))
        is Action.SetTimeSig -> s.copy(metronome = s.metronome.copy(sig = a.sig))

        is Action.SetSettingsTab -> (s.ui.top as? Overlay.Settings)?.let { s.replaceTop(it.copy(tab = a.tab)) } ?: open(s, Overlay.Settings(a.tab))
        is Action.SetCalibrationDbu -> s.copy(signal = s.signal.copy(calibrationDbu = a.dbu.coerceIn(-60.0, 60.0)))
        is Action.SetCalibrate -> s.copy(signal = s.signal.copy(calibrate = a.on))
        is Action.SetOutputMode -> if (a.mode !in 0..2 || (a.mode == 2 && s.amp?.outputLevelDbu == null)) s
        else s.copy(signal = s.signal.copy(outputMode = a.mode))
        is Action.SetLite -> s.copy(signal = s.signal.copy(lite = a.lite))
        is Action.SetAnimateArt -> s.copy(signal = s.signal.copy(animateArt = a.on))

        is Action.SetMidiChannel -> if (a.channel !in 0..16) s
        else s.copy(midi = s.midi.copy(channel = a.channel, lastOneChannel = if (a.channel > 0) a.channel else s.midi.lastOneChannel))
        is Action.SetRecallCc -> s.copy(midi = s.midi.copy(recallCc = a.cc.coerceIn(0, 119)))
        is Action.SetMidiBank -> s.copy(midi = s.midi.copy(bank = a.bank.coerceIn(0, 15)))
        is Action.SelectMidiDevice -> if (s.audio.midiDevices.none { it.id == a.deviceId }) s
        else s.audio { it.copy(midiDeviceId = a.deviceId, midiLost = false) }
        is Action.ScanBleMidi -> if (!s.host.bleFinds || s.audio.midiDevices.any { it.ble }) s.audio { it.copy(bleScanning = false) }
        else s.audio { it.copy(bleScanning = false, midiDevices = it.midiDevices + FakeEngine.bleController) }
        is Action.Midi -> midi(s, a.event)

        is Action.SelectInput -> if (s.audio.inputs.none { it.id == a.deviceId }) s
        else s.audio { it.copy(inputId = a.deviceId, inputChannel = 0, status = InterfaceStatus.OK) }
        is Action.SelectInputChannel -> {
            val dev = s.audio.inputs.firstOrNull { it.id == s.audio.inputId }
            if (dev == null || a.channel !in 0 until dev.channels) s else s.audio { it.copy(inputChannel = a.channel) }
        }
        is Action.SelectOutput -> if (s.audio.outputs.none { it.id == a.deviceId }) s else s.audio { it.copy(outputId = a.deviceId) }
        is Action.SetSampleRate -> if (a.rate !in s.audio.sampleRates) s else s.audio { it.copy(sampleRate = a.rate) }
        is Action.SetBuffer -> if (a.frames !in s.audio.buffers) s else s.audio { it.copy(bufferFrames = a.frames) }
        is Action.RequestMicPermission -> when {
            s.audio.permission == Permission.GRANTED -> s
            s.host.permissionGrantsOnRequest -> s.audio { it.copy(permission = Permission.GRANTED) }
            else -> s.audio { it.copy(permission = Permission.DENIED_PERMANENT) }
        }
        is Action.OpenAppSettings -> s.log("settings:app")
        is Action.RescanAudio -> if (s.audio.status != InterfaceStatus.NO_INTERFACE || !s.host.rescanFindsInterface) s
        else s.audio { a2 ->
            val inputs = if (a2.inputs.any { it.id == FakeEngine.usbInterface.id }) a2.inputs else listOf(FakeEngine.usbInterface) + a2.inputs
            val outputs = if (a2.outputs.any { it.id == FakeEngine.usbInterface.id }) a2.outputs else listOf(FakeEngine.usbInterface) + a2.outputs
            a2.copy(inputs = inputs, outputs = outputs, inputId = FakeEngine.usbInterface.id, outputId = FakeEngine.usbInterface.id, status = InterfaceStatus.OK)
        }
        is Action.UseBuiltinInput -> s.audio { it.copy(inputId = FakeEngine.builtinMic.id, outputId = FakeEngine.builtinSpeaker.id, inputChannel = 0, status = InterfaceStatus.OK) }
        is Action.ReconnectInterface -> if (s.audio.status != InterfaceStatus.UNPLUGGED) s
        else s.audio { it.copy(inputs = listOf(FakeEngine.usbInterface) + it.inputs.filter { d -> d.id != FakeEngine.usbInterface.id }, inputId = FakeEngine.usbInterface.id, status = InterfaceStatus.OK) }

        is Action.SetAutoUpdate -> s.copy(update = s.update.copy(auto = a.on))
        is Action.CheckUpdatesNow -> s.copy(update = s.update.copy(checks = s.update.checks + 1, available = s.host.updateOnCheck))
        is Action.OpenLink -> s.log("link:${a.url}")
        is Action.ShareLog -> s.log("share:volum.log")

        is Action.OpenBuilder -> openBuilder(s, a.editAmpId)
        is Action.BuilderSetName -> s.copy(builder = s.builder.copy(name = a.name.take(40)))
        is Action.BuilderSetFileCab -> s.builderFile(a.index) { it.copy(cabSlot = a.cabSlot.coerceIn(0, 3)) }
        is Action.BuilderSetFileChannel -> s.builderFile(a.index) { it.copy(channel = a.channel.coerceIn(1, 6)) }
        is Action.BuilderRemoveFile -> if (a.index !in s.builder.files.indices) s
        else s.copy(builder = s.builder.copy(files = s.builder.files.filterIndexed { i, _ -> i != a.index }))
        is Action.BuilderSetCabName -> if (a.slot !in 0..3 || a.name.isBlank()) s
        else s.copy(builder = s.builder.copy(cabNames = s.builder.cabNames.toMutableList().also { it[a.slot] = a.name.take(12) }))
        is Action.BuilderSetArt -> s.copy(builder = s.builder.copy(art = a.art.coerceIn(0, 5)))
        is Action.BuilderSave -> builderSave(s)
        is Action.RequestDeleteAmp -> s.library.amp(a.ampId)?.takeIf { it.custom }
            ?.let { s.ui(s.ui.copy(confirm = Confirm(ConfirmKind.DELETE_AMP, it.id, "Delete \"${it.name}\" and its presets?"))) } ?: s

        is Action.OpenIrConfig -> s.ui(s.ui.copy(irConfigOpen = if (s.ui.irConfigOpen == a.irId) null else a.irId))
        is Action.SetIrLevel -> s.ir(a.irId) { it.copy(levelDb = a.db.coerceIn(-20.0, 20.0)) }
        is Action.SetIrLowCut -> s.ir(a.irId) { it.copy(lowCutHz = a.hz.coerceIn(20.0, 500.0)) }
        is Action.SetIrHighCut -> s.ir(a.irId) { it.copy(highCutHz = a.hz.coerceIn(2000.0, 20000.0)) }
        is Action.RequestRenameIr -> s.library.ir(a.irId)?.let { open(s, Overlay.NameDialog(NamePurpose.RENAME_IR, it.name, it.id)) } ?: s
        is Action.RequestDeleteIr -> s.library.ir(a.irId)?.let { s.ui(s.ui.copy(confirm = Confirm(ConfirmKind.DELETE_IR, it.id, "Delete IR \"${it.name}\"?"))) } ?: s
        is Action.RequestRenamePreset -> s.library.preset(a.presetId)?.takeIf { !it.factory }
            ?.let { open(s, Overlay.NameDialog(NamePurpose.RENAME_PRESET, it.name, it.id)) } ?: s
        is Action.RequestRenamePedal -> s.library.pedals.firstOrNull { it.custom && it.index == a.index }
            ?.let { open(s, Overlay.NameDialog(NamePurpose.RENAME_PEDAL, it.name, it.index.toString())) } ?: s
        is Action.RequestDeletePedal -> s.library.pedals.firstOrNull { it.custom && it.index == a.index }
            ?.let { s.ui(s.ui.copy(confirm = Confirm(ConfirmKind.DELETE_PEDAL, it.index.toString(), "Delete pedal \"${it.name}\"?"))) } ?: s

        is Action.SetExportScope -> s.copy(packExport = s.packExport.copy(scope = a.scope))
        is Action.ToggleExportSound -> if (s.library.preset(a.presetId)?.factory != false) s
        else s.copy(packExport = s.packExport.copy(ticked = s.packExport.ticked.let { if (a.presetId in it) it - a.presetId else it + a.presetId }))
        is Action.SetExportAmp -> if (s.library.amp(a.ampId)?.custom != true) s else s.copy(packExport = s.packExport.copy(ampId = a.ampId))
        is Action.ToggleImportRow -> s.packImport?.let { pi ->
            val rows = pi.rows.map { if (it.id == a.rowId) it.copy(ticked = !it.ticked) else it }
            val merge = if (pi.merge == MergeMode.RESET && !rows.all { it.ticked }) MergeMode.OVERWRITE else pi.merge
            s.copy(packImport = pi.copy(rows = rows, merge = merge))
        } ?: s
        is Action.SetMerge -> s.packImport?.let { pi ->
            if (!pi.everything || (a.mode == MergeMode.RESET && !pi.allTicked)) s else s.copy(packImport = pi.copy(merge = a.mode))
        } ?: s
        is Action.SetRestoreMachine -> s.packImport?.takeIf { it.everything }?.let { s.copy(packImport = it.copy(restoreMachine = a.on)) } ?: s
        is Action.ConfirmImport -> confirmImport(s)

        is Action.PickFiles -> s
        is Action.FilesPicked -> filesPicked(s, a.kind, a.files)
    }

    private fun VoLumState.ui(ui: UiState) = copy(ui = ui)
    private fun VoLumState.lib(lib: Library) = copy(library = lib)
    private fun VoLumState.audio(f: (AudioState) -> AudioState) = copy(audio = f(audio))
    private fun VoLumState.log(entry: String) = copy(hostLog = hostLog + entry)
    private fun VoLumState.rig(f: (Rig) -> Rig) = copy(rigs = rigs + (ampId to f(rig)))
    private fun VoLumState.replaceTop(o: Overlay) = ui(ui.copy(overlays = ui.overlays.dropLast(1) + o))
    private inline fun <reified T : Overlay> VoLumState.pop(): VoLumState =
        if (ui.top is T) ui(ui.copy(overlays = ui.overlays.dropLast(1))) else this
    private fun VoLumState.builderFile(i: Int, f: (BuilderFile) -> BuilderFile) =
        if (i !in builder.files.indices) this else copy(builder = builder.copy(files = builder.files.mapIndexed { j, x -> if (j == i) f(x) else x }))
    private fun VoLumState.ir(id: String, f: (Ir) -> Ir) =
        if (library.ir(id) == null) this else lib(library.copy(irs = library.irs.map { if (it.id == id) f(it) else it }))

    fun open(s: VoLumState, o: Overlay): VoLumState {
        val stack = s.ui.overlays
        val next = if (stack.lastOrNull()?.let { it::class == o::class } == true) stack.dropLast(1) + o else stack + o
        val withState = when (o) {
            is Overlay.CustomAmpBuilder -> if (stack.lastOrNull() is Overlay.CustomAmpBuilder) s else s.copy(builder = BuilderState())
            else -> s
        }
        return withState.ui(withState.ui.copy(overlays = next))
    }

    private fun back(s: VoLumState): VoLumState = when {
        s.ui.confirm != null -> s.ui(s.ui.copy(confirm = null))
        s.ui.overlays.isNotEmpty() -> s.ui(s.ui.copy(overlays = s.ui.overlays.dropLast(1)))
        else -> s
    }

    private fun setView(s: VoLumState, v: View): VoLumState {
        var n = s
        if (v == View.PLAY) {
            if (n.locks.pre != null) n = setLock(storeLock(n, Section.PRE), Section.PRE, false)
            if (n.locks.post != null) n = setLock(storeLock(n, Section.POST), Section.POST, false)
        }
        return n.ui(n.ui.copy(view = v, overlays = emptyList(), buildTipSeen = n.ui.buildTipSeen || v == View.BUILD))
    }

    fun setParam(s: VoLumState, p: Param, raw: Double): VoLumState {
        val v = p.clamp(if (p.isSwitch || p.isEnum || p == Param.PITCH_SEMI || p == Param.NAM1_CAPTURE || p == Param.NAM2_CAPTURE) Math.round(raw).toDouble() else raw)
        val old = s.param(p)
        if (old == v) return s
        var n = s
        val modeKnobs = ModeParams.forModeParam(p)
        if (modeKnobs.isNotEmpty()) {
            val saved = modeKnobs.associateWith { s.param(it) }
            val memory = s.rig.modeMemory + ("${p.name}:${old.toInt()}" to saved)
            val restored = memory["${p.name}:${v.toInt()}"] ?: modeKnobs.associateWith { it.default }
            n = n.rig { it.copy(modeMemory = memory) }
            restored.forEach { (k, value) -> n = writeParam(n, k, value) }
        }
        n = writeParam(n, p, v)
        if (p == Param.DUAL_ON && v == 1.0 && !n.rig.dualPannedOnce) {
            n = writeParam(writeParam(n, Param.MAIN_PAN, -0.5), Param.SUPPORT_PAN, 0.5).rig { it.copy(dualPannedOnce = true) }
        }
        if (p == Param.DUAL_ON && v == 0.0) n = n.ui(n.ui.copy(lane = Lane.MAIN))
        return n
    }

    private fun writeParam(s: VoLumState, p: Param, v: Double): VoLumState = when {
        p.group == ParamGroup.PRE && s.locks.pre != null -> s.copy(locks = s.locks.copy(pre = s.locks.pre + (p to v)))
        p.group == ParamGroup.POST && s.locks.post != null -> s.copy(locks = s.locks.copy(post = s.locks.post + (p to v)))
        else -> s.rig { it.copy(params = it.params + (p to v), dirty = true) }
    }

    private fun groupOf(section: Section) = if (section == Section.PRE) ParamGroup.PRE else ParamGroup.POST

    private fun setLock(s: VoLumState, section: Section, locked: Boolean): VoLumState {
        if (section == Section.AMP) return s
        val snapshot = if (locked) Param.entries.filter { it.group == groupOf(section) }.associateWith { s.param(it) } else null
        return s.copy(locks = if (section == Section.PRE) s.locks.copy(pre = snapshot) else s.locks.copy(post = snapshot))
    }

    private fun storeLock(s: VoLumState, section: Section): VoLumState {
        val lock = (if (section == Section.PRE) s.locks.pre else s.locks.post) ?: return s
        if (!lockDiffers(s, section)) return s
        return s.rig { it.copy(params = it.params + lock, dirty = true) }
    }

    fun lockDiffers(s: VoLumState, section: Section): Boolean {
        val lock = (if (section == Section.PRE) s.locks.pre else s.locks.post) ?: return false
        return lock.any { (p, v) -> (s.rig.params[p] ?: p.default) != v }
    }

    private fun selectAmp(s: VoLumState, ampId: String): VoLumState {
        val amp = s.library.amp(ampId) ?: return s
        if (s.host.loadFailOnAmp == ampId) {
            return s.ui(s.ui.copy(banner = Banner(BannerKind.ERROR, "Could not load ${amp.name}. The previous amp keeps playing.")))
        }
        val rigs = if (ampId in s.rigs) s.rigs else s.rigs + (ampId to Catalog.defaultRig(amp))
        return s.copy(ampId = ampId, rigs = rigs).let { it.ui(it.ui.copy(lane = Lane.MAIN)) }.pop<Overlay.AmpPicker>()
    }

    private fun selectChannel(s: VoLumState, ch: Int): VoLumState {
        val amp = s.amp ?: return s
        if (ch !in 0 until amp.channels || ch == s.rig.channel) return s
        return s.rig {
            val cab = if (it.cab == Cab.CUSTOM_IR && ch !in amp.noCabChannels) stockCab(amp, ch) else it.cab
            it.copy(channel = ch, cab = cab, dirty = true)
        }
    }

    private fun selectSupportIr(s: VoLumState, irId: String): VoLumState {
        val amp = s.library.amp(s.rig.support.ampId) ?: return s
        if (s.library.ir(irId) == null || s.rig.support.channel !in amp.noCabChannels) return s
        return s.rig { it.copy(support = it.support.copy(cab = Cab.CUSTOM_IR, irId = irId), dirty = true) }.pop<Overlay.IrMenu>()
    }

    /** The real cab a lane takes when it leaves Custom IR: cab 1 on a bundled amp, a custom amp's first real cab on [channel], else No Cab. */
    fun stockCab(amp: Amp?, channel: Int): Cab =
        if (amp?.custom != true) Cab.G12
        else amp.files.filter { it.channel - 1 == channel && it.cabSlot > 0 }.minOfOrNull { it.cabSlot }?.let { Cab.entries[it] } ?: Cab.NONE

    /** Desktop "deleting a playing IR": keep the channel when a real cab covers it, else the custom amp's first real cab; No Cab only without one. */
    private fun irFallback(amp: Amp?, channel: Int): Pair<Int, Cab> {
        val here = stockCab(amp, channel)
        if (here != Cab.NONE || amp == null) return channel to here
        val first = amp.files.filter { it.cabSlot > 0 }.minByOrNull { it.channel } ?: return channel to Cab.NONE
        return (first.channel - 1) to Cab.entries[first.cabSlot]
    }

    private fun dropIr(lib: Library, ampId: String, r: Rig, irId: String): Rig {
        var n = r
        if (r.irId == irId) {
            val (ch, cab) = if (r.cab == Cab.CUSTOM_IR) irFallback(lib.amp(ampId), r.channel) else r.channel to r.cab
            n = n.copy(irId = null, channel = ch, cab = cab)
        }
        if (r.support.irId == irId) {
            val (ch, cab) = if (r.support.cab == Cab.CUSTOM_IR) irFallback(lib.amp(r.support.ampId), r.support.channel) else r.support.channel to r.support.cab
            n = n.copy(support = n.support.copy(irId = null, channel = ch, cab = cab))
        }
        return n
    }

    private fun dropSupportAmp(r: Rig, ampId: String): Rig =
        if (r.support.ampId != ampId) r else r.copy(support = SupportLane(), params = r.params + (Param.DUAL_ON to 0.0))

    private fun selectCab(s: VoLumState, cab: Cab): VoLumState {
        val amp = s.amp ?: return s
        if (cab == Cab.CUSTOM_IR && (s.rig.channel !in amp.noCabChannels || s.rig.irId == null)) return s
        return s.rig { it.copy(cab = cab, dirty = true) }
    }

    private fun stepPreset(s: VoLumState, delta: Int): VoLumState {
        val list = s.library.presetsFor(s.ampId)
        if (list.isEmpty()) return s
        val i = list.indexOfFirst { it.id == s.rig.presetId }
        val next = if (i < 0) (if (delta > 0) 0 else list.lastIndex) else Math.floorMod(i + delta, list.size)
        return loadPreset(s, list[next].id)
    }

    private fun loadPreset(s: VoLumState, id: String): VoLumState {
        val p = s.library.preset(id) ?: return s
        val withAmp = if (p.ampId != s.ampId) selectAmp(s, p.ampId) else s
        if (withAmp.ampId != p.ampId) return withAmp
        return withAmp.rig { p.rig.copy(presetId = p.id, dirty = false, modeMemory = it.modeMemory) }
    }

    private fun uniqueName(base: String, taken: Collection<String>): String {
        val lower = taken.map { it.lowercase() }.toSet()
        if (base.lowercase() !in lower) return base
        var n = 2
        while ("$base $n".lowercase() in lower) n++
        return "$base $n"
    }

    private fun nextUserPresetId(lib: Library): String =
        "u:" + ((lib.presets.mapNotNull { it.id.removePrefix("u:").toIntOrNull().takeIf { _ -> it.id.startsWith("u:") } }.maxOrNull() ?: 0) + 1)

    private fun savePreset(s: VoLumState, name: String): Pair<VoLumState, String> {
        val current = s.currentPreset
        if (current != null && !current.factory && current.name.equals(name, ignoreCase = true)) {
            val updated = current.copy(rig = s.rig.sound())
            val n = s.lib(s.library.copy(presets = s.library.presets.map { if (it.id == current.id) updated else it }))
                .rig { it.copy(dirty = false) }
            return n to current.id
        }
        val userNames = s.library.presets.filter { it.ampId == s.ampId && !it.factory }.map { it.name }
        val id = nextUserPresetId(s.library)
        val preset = Preset(id, s.ampId, uniqueName(name, userNames), factory = false, rig = s.rig.sound())
        val oldPresetId = s.rig.presetId
        var lib = s.library.copy(presets = s.library.presets + preset)
        val live = lib.live
        if (s.ui.view == View.PLAY && live != null && oldPresetId != null && lib.sounds[live]?.presetId == oldPresetId) {
            lib = lib.copy(sounds = lib.sounds + (live to Sound(s.ampId, id)))
        }
        return s.lib(lib).rig { it.copy(presetId = id, dirty = false) } to id
    }

    fun nameSuggestion(s: VoLumState): String = s.currentPreset?.takeIf { !it.factory }?.name ?: "New Preset"

    private fun commitName(s: VoLumState): VoLumState {
        val dlg = s.ui.top as? Overlay.NameDialog ?: return s
        val name = dlg.text.trim()
        if (name.isEmpty()) return s
        val closed = s.pop<Overlay.NameDialog>()
        return when (dlg.purpose) {
            NamePurpose.SAVE_PRESET -> savePreset(closed, name).first
            NamePurpose.ADD_SOUND -> {
                val (saved, id) = savePreset(closed, name)
                val free = saved.library.firstFreeProgram() ?: return saved
                saved.lib(saved.library.copy(sounds = saved.library.sounds + (free to Sound(saved.ampId, id)), live = free))
            }
            NamePurpose.RENAME_PRESET -> {
                val p = s.library.preset(dlg.targetId) ?: return closed
                val taken = s.library.presets.filter { it.ampId == p.ampId && !it.factory && it.id != p.id }.map { it.name }
                val newName = uniqueName(name, taken)
                closed.lib(closed.library.copy(presets = closed.library.presets.map { if (it.id == p.id) it.copy(name = newName) else it }))
            }
            NamePurpose.RENAME_IR -> {
                val taken = s.library.irs.filter { it.id != dlg.targetId }.map { it.name }
                closed.ir(dlg.targetId ?: "") { it.copy(name = uniqueName(name, taken)) }
            }
            NamePurpose.RENAME_PEDAL -> {
                val idx = dlg.targetId?.toIntOrNull() ?: return closed
                val taken = s.library.pedals.filter { it.custom && it.index != idx }.map { it.name }
                closed.lib(closed.library.copy(pedals = closed.library.pedals.map { if (it.custom && it.index == idx) it.copy(name = uniqueName(name, taken)) else it }))
            }
        }
    }

    private fun acceptConfirm(s: VoLumState, c: Confirm): VoLumState = when (c.kind) {
        ConfirmKind.OVERWRITE_PRESET -> {
            val target = s.library.preset(c.targetId)
            if (target == null) s
            else s.lib(s.library.copy(presets = s.library.presets.map { if (it.id == target.id) it.copy(rig = s.rig.sound()) else it }))
                .rig { it.copy(presetId = target.id, dirty = false) }.pop<Overlay.PresetMenu>()
        }
        ConfirmKind.DELETE_PRESET -> s.lib(s.library.copy(presets = s.library.presets.filter { it.id != c.targetId }))
            .copy(rigs = s.rigs.mapValues { (_, r) -> if (r.presetId == c.targetId) r.copy(presetId = null) else r })
        ConfirmKind.DELETE_AMP -> {
            val lib = s.library.copy(
                amps = s.library.amps.filter { it.id != c.targetId },
                presets = s.library.presets.filter { it.ampId != c.targetId },
            )
            val ampId = if (s.ampId == c.targetId) Catalog.ampId(0) else s.ampId
            val rigs = (s.rigs - c.targetId).let { if (ampId in it) it else it + (ampId to Catalog.defaultRig(lib.amp(ampId)!!)) }
            val n = s.copy(
                library = lib.copy(presets = lib.presets.map { it.copy(rig = dropSupportAmp(it.rig, c.targetId)) }),
                ampId = ampId,
                rigs = rigs.mapValues { (_, r) -> dropSupportAmp(r, c.targetId) },
            )
            if (n.dual) n else n.ui(n.ui.copy(lane = Lane.MAIN))
        }
        ConfirmKind.DELETE_IR -> {
            val lib = s.library.copy(irs = s.library.irs.filter { it.id != c.targetId })
            s.copy(
                library = lib.copy(presets = lib.presets.map { it.copy(rig = dropIr(lib, it.ampId, it.rig, c.targetId)) }),
                rigs = s.rigs.mapValues { (ampId, r) -> dropIr(lib, ampId, r, c.targetId) },
            )
        }
        ConfirmKind.DELETE_PEDAL -> {
            val idx = c.targetId.toDoubleOrNull() ?: -1.0
            val lib = s.library.copy(pedals = s.library.pedals.filter { !(it.custom && it.index.toDouble() == idx) })
            val clear = { m: Map<Param, Double> ->
                m.mapValues { (p, v) -> if ((p == Param.NAM1_CAPTURE || p == Param.NAM2_CAPTURE) && v == idx) 0.0 else v }
            }
            s.copy(
                library = lib,
                rigs = s.rigs.mapValues { (_, r) -> r.copy(params = clear(r.params)) },
                locks = s.locks.copy(pre = s.locks.pre?.let(clear)),
            )
        }
    }

    private fun recall(s: VoLumState, program: Int): VoLumState {
        if (!s.library.soundValid(program)) return s
        val sound = s.library.sounds.getValue(program)
        val loaded = loadPreset(s, sound.presetId)
        if (loaded.ampId != sound.ampId) return loaded
        return loaded.lib(loaded.library.copy(live = program))
    }

    private fun stepSound(s: VoLumState, delta: Int): VoLumState {
        val programs = s.library.sounds.keys.filter { s.library.soundValid(it) }.sorted()
        if (programs.isEmpty()) return s
        val live = s.library.live
        val next = when {
            live == null -> if (delta > 0) programs.first() else programs.last()
            delta > 0 -> programs.firstOrNull { it > live } ?: programs.first()
            else -> programs.lastOrNull { it < live } ?: programs.last()
        }
        return recall(s, next)
    }

    private fun addThisSound(s: VoLumState): VoLumState {
        val preset = s.currentPreset
        if (preset == null) return open(s, Overlay.NameDialog(NamePurpose.ADD_SOUND, "New Preset"))
        val free = s.library.firstFreeProgram() ?: return open(s, Overlay.SoundPicker(s.library.live ?: 0, replace = true))
        return s.lib(s.library.copy(sounds = s.library.sounds + (free to Sound(s.ampId, preset.id)), live = free))
    }

    private fun moveSound(s: VoLumState, program: Int, delta: Int): VoLumState {
        val target = program + delta
        val from = s.library.sounds[program] ?: return s
        if (target !in 0..127) return s
        val to = s.library.sounds[target]
        var sounds = s.library.sounds - program + (target to from)
        if (to != null) sounds = sounds + (program to to)
        val live = when (s.library.live) {
            program -> target
            target -> if (to != null) program else s.library.live
            else -> s.library.live
        }
        return s.lib(s.library.copy(sounds = sounds, live = live))
    }

    private fun editStomp(s: VoLumState, p: Param): VoLumState {
        val base = setView(s, View.BUILD)
        return when (p) {
            Param.PITCH_ON -> reduce(base, Action.FocusPre(PreBlock.PITCH))
            Param.COMP_ON -> reduce(base, Action.FocusPre(PreBlock.COMP))
            Param.NAM1_ON -> reduce(base, Action.FocusPre(PreBlock.NAM1))
            Param.NAM2_ON -> reduce(base, Action.FocusPre(PreBlock.NAM2))
            Param.CHORUS_ON -> reduce(base, Action.FocusPost(PostBlock.CHORUS))
            Param.DELAY_ON -> reduce(base, Action.FocusPost(PostBlock.DELAY))
            Param.REVERB_ON -> reduce(base, Action.FocusPost(PostBlock.REVERB))
            Param.TREM_ON -> reduce(base, Action.FocusPost(PostBlock.TREM))
            else -> base
        }
    }

    private fun midi(s: VoLumState, e: MidiEvent): VoLumState {
        if (s.midi.channel != 0 && e.channel != s.midi.channel) return s
        return when (e) {
            is MidiEvent.ProgramChange -> recall(s, e.program)
            is MidiEvent.ControlChange -> if (e.cc == s.midi.recallCc) recall(s, e.value) else s
        }
    }

    private fun openBuilder(s: VoLumState, editId: String?): VoLumState {
        val amp = s.library.amp(editId)?.takeIf { it.custom }
        val builder = if (amp == null) BuilderState()
        else BuilderState(amp.id, amp.name, amp.files.map { BuilderFile(it.name, it.cabSlot, it.channel) }, amp.cabNames, amp.art)
        val opened = open(s, Overlay.CustomAmpBuilder)
        return opened.copy(builder = builder)
    }

    private fun builderSave(s: VoLumState): VoLumState {
        val b = s.builder
        if (b.name.isBlank() || b.files.isEmpty()) return s
        val channels = b.files.maxOf { it.channel }
        val noCab = b.files.filter { it.cabSlot == 0 }.map { it.channel - 1 }.toSet()
        val id = b.editAmpId ?: ("c:" + ((s.library.amps.mapNotNull { it.id.removePrefix("c:").toIntOrNull().takeIf { _ -> it.id.startsWith("c:") } }.maxOrNull() ?: 0) + 1))
        val amp = Amp(id, b.name.trim(), channels, custom = true, art = b.art, noCabChannels = noCab,
            files = b.files.map { AmpFile(it.name, it.cabSlot, it.channel) }, cabNames = b.cabNames)
        val amps = if (b.editAmpId != null) s.library.amps.map { if (it.id == id) amp else it } else s.library.amps + amp
        val saved = s.lib(s.library.copy(amps = amps)).pop<Overlay.CustomAmpBuilder>()
        return if (b.editAmpId == null) selectAmp(saved, id) else saved
    }

    private fun filesPicked(s: VoLumState, kind: PickKind, files: List<PickedFile>): VoLumState {
        if (files.isEmpty()) return s
        val invalid = files.filter { !it.valid }
        val valid = files.filter { it.valid }
        val withBanner = if (invalid.isEmpty()) s else s.ui(s.ui.copy(banner = Banner(BannerKind.ERROR,
            when (kind) {
                PickKind.IR_FILES -> "File is not a WAV file: ${invalid.joinToString { it.name }}"
                PickKind.PACK_OPEN -> "This Pack is damaged. Nothing was changed."
                else -> "File is not a NAM capture: ${invalid.joinToString { it.name }}"
            })))
        return when (kind) {
            PickKind.NAM_AMP_FILES -> withBanner.copy(builder = withBanner.builder.copy(files = withBanner.builder.files + valid.map { FakeEngine.parseAmpFile(it.name) }))
            PickKind.IR_FILES -> {
                var lib = withBanner.library
                valid.forEach { f ->
                    val base = FakeEngine.shortenMiddle(f.name.substringBeforeLast('.'))
                    if (lib.irs.any { it.id == "ir:${f.name}:${f.bytes}" }) return@forEach
                    lib = lib.copy(irs = lib.irs + Ir("ir:${f.name}:${f.bytes}", uniqueName(base, lib.irs.map { it.name })))
                }
                withBanner.lib(lib)
            }
            PickKind.PEDAL_FILES -> {
                var lib = withBanner.library
                valid.forEach { f ->
                    if (lib.pedals.count { it.custom } >= 64) return@forEach
                    val index = (lib.pedals.filter { it.custom }.maxOfOrNull { it.index } ?: 63) + 1
                    val base = FakeEngine.shortenMiddle(f.name.substringBeforeLast('.'))
                    lib = lib.copy(pedals = lib.pedals + Pedal(index, uniqueName(base, lib.pedals.filter { it.custom }.map { it.name }), true))
                }
                withBanner.lib(lib)
            }
            PickKind.PACK_SAVE -> withBanner.copy(packExport = withBanner.packExport.copy(savedAs = valid.first().name)).log("file:${valid.first().name}")
            PickKind.PACK_OPEN -> open(withBanner.copy(packImport = FakeEngine.packPreview(withBanner, files.first())), Overlay.PackImport)
        }
    }

    private fun confirmImport(s: VoLumState): VoLumState {
        val pi = s.packImport ?: return s
        if (pi.damaged || pi.refused || pi.imported) return s
        var lib = s.library.copy(packBackups = s.library.packBackups + 1)
        val take = pi.rows.filter { it.ticked && it.outcome != PackOutcome.SKIP && !(pi.merge == MergeMode.ADD && it.outcome == PackOutcome.REPLACE) }
        if (pi.merge == MergeMode.RESET) {
            lib = lib.copy(irs = emptyList(), pedals = lib.pedals.filter { !it.custom }, presets = lib.presets.filter { it.factory })
        }
        take.forEach { row ->
            lib = when (row.kind) {
                "ir" -> if (lib.irs.any { it.id == row.id }) lib.copy(irs = lib.irs.map { if (it.id == row.id) it.copy(name = row.name) else it })
                else lib.copy(irs = lib.irs + Ir(row.id, row.name))
                "pedal" -> if (lib.pedals.any { it.custom && it.name == row.name }) lib
                else lib.copy(pedals = lib.pedals + Pedal((lib.pedals.filter { it.custom }.maxOfOrNull { it.index } ?: 63) + 1, row.name, true))
                "preset" -> if (lib.presets.any { it.id == row.id }) lib.copy(presets = lib.presets.map { if (it.id == row.id) it.copy(name = row.name) else it })
                else lib.copy(presets = lib.presets + Preset(row.id, s.ampId, row.name, false, s.rig.sound()))
                else -> lib
            }
        }
        val imported = s.copy(library = lib, packImport = pi.copy(imported = true))
        return imported.ui(imported.ui.copy(banner = Banner(BannerKind.INFO, "Pack imported."))).pop<Overlay.PackImport>()
    }
}
