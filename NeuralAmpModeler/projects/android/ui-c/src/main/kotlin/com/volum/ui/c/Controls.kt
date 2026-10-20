package com.volum.ui.c

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.design.components.VoLumTextField
import com.volum.design.components.volumFieldFrame
import com.volum.design.components.volumKnobFace
import com.volum.design.theme.VFitText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.roundToInt

/** How a param reads on its knob, and how typed text maps back to it. */
internal object Fmt {
    private fun isPercent(p: Param) = p.min == 0.0 && p.max <= 1.0 && p.unit.isEmpty()

    fun of(p: Param, v: Double): String = when {
        p == Param.OUTPUT && v <= p.min -> "-inf"
        p == Param.SUPPORT_OUTPUT && v <= p.min -> "-inf"
        p == Param.MAIN_PAN || p == Param.SUPPORT_PAN -> pan(v)
        p.unit == "dB" -> "%.1f dB".format(v)
        p.unit == "ms" -> if (v < 10) "%.1f ms".format(v) else "%.0f ms".format(v)
        p.unit == "Hz" -> if (v >= 1000) "%.2f kHz".format(v / 1000) else if (p.max <= 20) "%.1f Hz".format(v) else "%.0f Hz".format(v)
        p.unit == "s" -> "%.1f s".format(v)
        p.unit == "st" -> "%+d st".format(v.roundToInt())
        isPercent(p) -> "%.0f%%".format(v * 100)
        else -> "%.1f".format(v)
    }

    fun pan(v: Double): String = when {
        abs(v) < 0.02 -> "C"
        v < 0 -> "L %.0f".format(-v * 100)
        else -> "R %.0f".format(v * 100)
    }

    fun parse(p: Param, text: String): Double? {
        val n = text.replace(',', '.').filter { it.isDigit() || it == '.' || it == '-' || it == '+' }.toDoubleOrNull() ?: return null
        return when {
            isPercent(p) && n > 1.0 -> n / 100.0
            p == Param.MAIN_PAN || p == Param.SUPPORT_PAN -> if (abs(n) > 1.0) n / 100.0 else n
            else -> n
        }
    }
}

/** A knob bound to one rig param; SUPPORT lane knobs read teal. */
@Composable
internal fun ParamKnob(ui: Ui, p: Param, ids: List<String>, label: String = p.label, support: Boolean = false, enabled: Boolean = true) {
    Knob(
        label = label,
        value = ui.s.param(p),
        min = p.min,
        max = p.max,
        ids = ids,
        format = { Fmt.of(p, it) },
        parse = { Fmt.parse(p, it) },
        onSet = { ui.d(Action.SetParam(p, it)) },
        onReset = { ui.d(Action.ResetParam(p)) },
        support = support,
        enabled = enabled,
    )
}

/**
 * The VoLum dial (`:design` knob face) with C's fingertip model: vertical drag sets
 * the value (full range over 220 dp); sliding the finger more than 64 dp sideways
 * while dragging drops to a quarter speed (the ring shows it). Double tap resets,
 * long press opens exact entry. While dragging, the value rides in an amber bubble
 * above the dial, where the finger cannot cover it.
 */
@Composable
internal fun Knob(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    ids: List<String>,
    format: (Double) -> String,
    parse: (String) -> Double?,
    onSet: (Double) -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier,
    support: Boolean = false,
    enabled: Boolean = true,
    dial: Dp = 60.dp,
    width: Dp = 84.dp,
    readout: Boolean = true,
) {
    var dragging by remember { mutableStateOf(false) }
    var fine by remember { mutableStateOf(false) }
    var editing by remember { mutableStateOf(false) }
    val current by rememberUpdatedState(value)
    val set by rememberUpdatedState(onSet)
    val reset by rememberUpdatedState(onReset)
    val view = LocalView.current
    val span = (max - min).takeIf { it > 0 } ?: 1.0
    val norm = ((value - min) / span).toFloat().coerceIn(0f, 1f)
    val normState = rememberUpdatedState(norm)
    val dragState = rememberUpdatedState(dragging)
    val fineState = rememberUpdatedState(fine)
    val accent = if (support) VoLumColors.KnobTeal else VoLumColors.KnobGold
    val accentHi = if (support) VoLumColors.KnobTealHi else VoLumColors.KnobGoldHi
    val face = remember(support) { Modifier.volumKnobFace({ normState.value }, accent, accentHi, { dragState.value }, { fineState.value }) }
    val text = format(value)
    Column(modifier.width(width).alpha(if (enabled) 1f else 0.4f), horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.fillMaxWidth().heightIn(min = 20.dp), contentAlignment = Alignment.Center) {
            if (dragging) {
                Box(Modifier.background(VoLumColors.Amber, RoundedCornerShape(3.dp)).padding(horizontal = 6.dp, vertical = 1.dp)) {
                    VFitText(if (fine) "FINE $text" else text, style = VoLumType.label(12.sp, VoLumColors.AmberInk), minSize = 8.sp)
                }
            } else {
                VFitText(label, style = VoLumType.KnobLabel, minSize = 9.sp, textAlign = TextAlign.Center)
            }
        }
        Spacer(
            Modifier
                .padding(vertical = 2.dp)
                .size(dial)
                .pointerInput(min, max, enabled) {
                    if (!enabled) return@pointerInput
                    var n = 0f
                    var dx = 0f
                    var tick = 0
                    detectDragGestures(
                        onDragStart = { n = ((current - min) / span).toFloat().coerceIn(0f, 1f); dx = 0f; tick = floor(n * 20).toInt(); dragging = true; fine = false },
                        onDragEnd = { dragging = false; fine = false },
                        onDragCancel = { dragging = false; fine = false },
                    ) { change, amount ->
                        change.consume()
                        dx += amount.x
                        val nowFine = abs(dx) > 64.dp.toPx()
                        if (nowFine && !fine) view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                        fine = nowFine
                        n = (n - amount.y / 220.dp.toPx() * (if (fine) 0.25f else 1f)).coerceIn(0f, 1f)
                        val t = floor(n * 20).toInt()
                        if (t != tick) { view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK); tick = t }
                        set(min + n * span)
                    }
                }
                .pointerInput(enabled) { if (enabled) detectTapGestures(onDoubleTap = { reset() }, onLongPress = { editing = true }) }
                .semantics {
                    contentDescription = label
                    stateDescription = text
                    progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                    if (enabled) {
                        setProgress { v -> set(v.toDouble().coerceIn(min, max)); true }
                        setText { t -> parse(t.text)?.let { set(it.coerceIn(min, max)); true } ?: false }
                        customActions = listOf(CustomAccessibilityAction("Reset") { reset(); true })
                    }
                }
                .ids(ids)
                .then(face),
        )
        Box(Modifier.fillMaxWidth().heightIn(min = 20.dp), contentAlignment = Alignment.Center) {
            if (editing) {
                ExactEntry(value, format, parse) { v -> v?.let { set(it.coerceIn(min, max)) }; editing = false }
            } else if (readout) {
                VFitText(text, style = VoLumType.label(13.sp, if (dragging) VoLumColors.Amber else if (support) VoLumColors.Teal else VoLumColors.TextBright), minSize = 9.sp, textAlign = TextAlign.Center)
            }
        }
    }
}

@Composable
private fun ExactEntry(value: Double, format: (Double) -> String, parse: (String) -> Double?, onDone: (Double?) -> Unit) {
    var text by remember { mutableStateOf(format(value).filter { it.isDigit() || it == '.' || it == '-' || it == '+' }) }
    val focus = remember { FocusRequester() }
    LaunchedEffect(Unit) { runCatching { focus.requestFocus() } }
    BasicTextField(
        value = text,
        onValueChange = { text = it },
        singleLine = true,
        textStyle = VoLumType.label(13.sp, VoLumColors.TextBright).copy(textAlign = TextAlign.Center),
        cursorBrush = SolidColor(VoLumColors.TextBright),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone(parse(text)) }),
        modifier = Modifier.width(76.dp).height(24.dp).volumFieldFrame(true).focusRequester(focus),
    )
}

/** The `:design` name field; it keeps its own text so partial input survives a recomposition. */
@Composable
internal fun Field(
    value: String,
    ids: List<String>,
    onChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    numeric: Boolean = false,
    onDone: () -> Unit = {},
    hint: String? = null,
    autofocus: Boolean = false,
    key: Any? = Unit,
) {
    var text by remember(key) { mutableStateOf(value) }
    val focus = remember { FocusRequester() }
    if (autofocus) LaunchedEffect(Unit) { runCatching { focus.requestFocus() } }
    VoLumTextField(
        text,
        { text = it; onChange(it) },
        modifier.focusRequester(focus).ids(ids),
        placeholder = hint,
        keyboardOptions = KeyboardOptions(keyboardType = if (numeric) KeyboardType.Decimal else KeyboardType.Text, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone() }),
    )
}
