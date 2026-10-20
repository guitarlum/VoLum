package com.volum.core

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * The single source of truth a candidate UI renders and dispatches to. Domain
 * state ([state]) changes only through [dispatch]; per-frame engine telemetry
 * ([telemetry]) is a separate flow so meters and art do not recompose the whole UI.
 */
class VoLumStore(initial: VoLumState, energyOverride: Double? = null) {
    private val _state = MutableStateFlow(initial)
    val state: StateFlow<VoLumState> = _state.asStateFlow()

    var energyOverride: Double? = energyOverride
        set(value) {
            field = value
            tick(_telemetry.value.timeMs)
        }

    private val _telemetry = MutableStateFlow(FakeEngine.telemetry(initial, 0, energyOverride))
    val telemetry: StateFlow<Telemetry> = _telemetry.asStateFlow()

    val current: VoLumState get() = _state.value

    fun dispatch(action: Action) {
        _state.update { s ->
            when (action) {
                is Action.PickFiles -> Reducer.reduce(s, Action.FilesPicked(action.kind, FakeEngine.answerPick(s, action.kind)))
                else -> Reducer.reduce(s, action)
            }
        }
        tick(_telemetry.value.timeMs)
    }

    fun tick(timeMs: Long) {
        _telemetry.value = FakeEngine.telemetry(_state.value, timeMs, energyOverride)
    }

    companion object {
        fun forScreen(screen: ScreenId, fixture: String = ScreenRegistry.spec(screen).fixture): VoLumStore {
            val f = Fixtures.spec(fixture)
            return VoLumStore(ScreenRegistry.place(f.build(), screen), f.energy)
        }
    }
}
