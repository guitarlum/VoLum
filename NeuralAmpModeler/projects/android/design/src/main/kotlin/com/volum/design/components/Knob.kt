package com.volum.design.components

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/** Desktop dial travel: -135..135 degrees, 0 at 12 o'clock. */
private const val A1 = -135f
private const val A2 = 135f

/** Full sweep for a vertical drag: IVKnob's default gearing over a ~50 px knob. */
private val SWEEP = 200.dp

/** Hold still this long to enter fine adjust (the desktop's Shift-drag, 10x slower). */
private const val FINE_HOLD_MS = 380L

/** Two taps within this window reset to default (the desktop's double-click). */
private const val DOUBLE_TAP_MS = 300L

/**
 * The VoLum dial (`VoLumDialKnobControl`): tick nubs, value arc with an
 * active glow, metallic top-lit cap with accent rim, accent pointer. Gold for
 * MAIN, teal for [support].
 *
 * Touch model: drag vertically anywhere on the knob (label, dial or value)
 * for the full range over 200 dp; hold still first to enter fine adjust (10x,
 * ring shown, haptic); double-tap resets to [default]. A haptic tick marks
 * every twentieth of the range and the default. Horizontal drags are left to
 * the parent, so a knob row inside a pager or list does not grab swipes.
 *
 * Semantics for the harness: `ProgressBarRangeInfo` + `SetProgress` over
 * [range], `SetText` when [parseEntry] is given, a "Reset" custom action.
 * The caller's [modifier] (feature ids) lands on that same merged node.
 */
@Composable
fun VoLumKnob(
    value: Double,
    onValueChange: (Double) -> Unit,
    range: ClosedFloatingPointRange<Double>,
    default: Double,
    label: String,
    valueText: String,
    modifier: Modifier = Modifier,
    support: Boolean = false,
    enabled: Boolean = true,
    dialSize: Dp = 44.dp,
    steps: Int = 0,
    parseEntry: ((String) -> Double?)? = null,
) {
    val view = LocalView.current
    val current by rememberUpdatedState(value)
    val change by rememberUpdatedState(onValueChange)
    var fine by remember { mutableStateOf(false) }
    var dragging by remember { mutableStateOf(false) }
    val span = (range.endInclusive - range.start).takeIf { it > 0 } ?: 1.0

    fun quantize(v: Double): Double {
        val c = v.coerceIn(range.start, range.endInclusive)
        if (steps <= 0) return c
        val stepSize = span / (steps + 1)
        return range.start + Math.round((c - range.start) / stepSize) * stepSize
    }

    val norm = ((value - range.start) / span).toFloat().coerceIn(0f, 1f)
    val accent = if (support) VoLumColors.KnobTeal else VoLumColors.KnobGold
    val accentHi = if (support) VoLumColors.KnobTealHi else VoLumColors.KnobGoldHi
    val active = dragging || fine

    Column(
        modifier
            .widthIn(min = 56.dp)
            .alpha(if (enabled) 1f else 0.4f)
            .semantics(mergeDescendants = true) {
                contentDescription = label
                stateDescription = valueText
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), range.start.toFloat()..range.endInclusive.toFloat(), steps)
                if (enabled) {
                    setProgress { v -> change(quantize(v.toDouble())); true }
                    customActions = listOf(CustomAccessibilityAction("Reset") { change(default); true })
                    if (parseEntry != null) setText { t -> parseEntry(t.text)?.let { change(quantize(it)); true } ?: false }
                } else {
                    disabled()
                }
            }
            .pointerInput(enabled, range, steps) {
                if (!enabled) return@pointerInput
                val sweepPx = SWEEP.toPx()
                var lastTapUp = 0L
                awaitEachGesture {
                    val down = awaitFirstDown(requireUnconsumed = false)
                    val start = current
                    var moved = 0f
                    var travelled = 0f
                    var decided = false
                    var vertical = false
                    var acc = start
                    var lastTick = floor(((start - range.start) / span) * 20).toInt()
                    fine = false
                    while (true) {
                        val ev = awaitPointerEvent()
                        val c = ev.changes.firstOrNull { it.id == down.id } ?: break
                        if (!c.pressed) {
                            val now = c.uptimeMillis
                            if (!decided && travelled < viewConfiguration.touchSlop) {
                                if (now - lastTapUp < DOUBLE_TAP_MS) {
                                    change(default)
                                    view.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK)
                                    lastTapUp = 0L
                                } else lastTapUp = now
                            }
                            break
                        }
                        val d = c.positionChange()
                        travelled += abs(d.x) + abs(d.y)
                        if (!decided) {
                            if (!fine && c.uptimeMillis - down.uptimeMillis > FINE_HOLD_MS && travelled < viewConfiguration.touchSlop) {
                                fine = true
                                view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                            }
                            moved += d.y
                            if (abs(c.position.y - down.position.y) > viewConfiguration.touchSlop) {
                                decided = true
                                vertical = true
                                dragging = true
                            } else if (abs(c.position.x - down.position.x) > viewConfiguration.touchSlop && !fine) {
                                decided = true
                                vertical = false
                            }
                        }
                        if (decided && !vertical) break
                        if (decided && vertical) {
                            c.consume()
                            val gear = if (fine) 10.0 else 1.0
                            acc = (acc - d.y / sweepPx / gear * span).coerceIn(range.start, range.endInclusive)
                            val next = quantize(acc)
                            if (next != current) {
                                change(next)
                                val tick = floor(((next - range.start) / span) * 20).toInt()
                                val crossedDefault = (current - default) * (next - default) <= 0 && next != current
                                if (tick != lastTick || crossedDefault) {
                                    view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                                    lastTick = tick
                                }
                            }
                        }
                    }
                    dragging = false
                    fine = false
                }
            },
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        VText(label, style = VoLumType.KnobLabel, textAlign = TextAlign.Center)
        KnobDial(norm, accent, accentHi, active, fine, Modifier.size(dialSize))
        VText(valueText, style = VoLumType.KnobValue, color = if (support) VoLumColors.Teal else VoLumColors.TextBright, textAlign = TextAlign.Center)
    }
}

/** The dial alone, for places that draw their own label (PAN on the hero lanes). */
@Composable
fun KnobDial(norm: Float, accent: Color, accentHi: Color, active: Boolean, ring: Boolean, modifier: Modifier = Modifier) {
    Spacer(
        modifier.drawWithCache {
            val r = size.minDimension / 2f
            val cx = size.width / 2f
            val cy = size.height / 2f
            val px = 1.dp.toPx()
            val bodyR = r - 5 * px
            val arcR = r - 2 * px
            val tickOff = ic(70, 200, 162, 78)
            val trackCol = ic(46, 200, 162, 78)
            val body = Brush.radialGradient(listOf(ic(255, 60, 60, 73), ic(255, 15, 15, 21)), center = Offset(cx, cy - bodyR * 0.45f), radius = bodyR * 1.55f)
            val shadow = ic(150, 0, 0, 0)
            val sheen = ic(34, 255, 255, 255)
            val arcTopLeft = Offset(cx - arcR, cy - arcR)
            val arcSize = Size(arcR * 2, arcR * 2)
            val track = Stroke(2 * px)
            val glowStroke = Stroke(6 * px, cap = StrokeCap.Butt)
            val valueStroke = Stroke(2.6f * px)
            val rim = Stroke(1.2f * px)
            val thin = Stroke(px)
            val ringStroke = Stroke(1.5f * px)
            onDrawBehind {
                val angle = A1 + norm * (A2 - A1)
                for (i in 0 until 11) {
                    val t = A1 + (A2 - A1) * (i / 10f)
                    val major = i % 5 == 0
                    val passed = t <= angle + 0.5f
                    val rad = Math.toRadians((t - 90f).toDouble())
                    val c = cos(rad).toFloat()
                    val s = sin(rad).toFloat()
                    val r0 = r - (if (major) 1.5f else 0.5f) * px
                    val r1 = r + (if (major) 2f else 1f) * px
                    drawLine(if (passed) accent.copy(alpha = 0.9f) else tickOff, Offset(cx + c * r0, cy + s * r0), Offset(cx + c * r1, cy + s * r1), (if (major) 1.4f else 1f) * px)
                }
                drawArc(trackCol, A1 - 90f, A2 - A1, false, arcTopLeft, arcSize, style = track)
                if (active) drawArc(accentHi.copy(alpha = 0.28f), A1 - 90f, angle - A1, false, arcTopLeft, arcSize, style = glowStroke)
                drawArc(if (active) accentHi else accent, A1 - 90f, angle - A1, false, arcTopLeft, arcSize, style = valueStroke)
                drawCircle(shadow, bodyR, Offset(cx, cy + 1.5f * px))
                drawCircle(body, bodyR, Offset(cx, cy))
                drawCircle(accent.copy(alpha = if (active) 0.7f else 0.4f), bodyR, Offset(cx, cy), style = rim)
                drawCircle(sheen, bodyR - 1.5f * px, Offset(cx, cy), style = thin)
                val rad = Math.toRadians((angle - 90f).toDouble())
                val c = cos(rad).toFloat()
                val s = sin(rad).toFloat()
                val ink = if (active) accentHi else accent
                drawLine(ink, Offset(cx + c * bodyR * 0.26f, cy + s * bodyR * 0.26f), Offset(cx + c * bodyR * 0.86f, cy + s * bodyR * 0.86f), 2.4f * px)
                drawCircle(ink, 2.6f * px, Offset(cx + c * bodyR * 0.86f, cy + s * bodyR * 0.86f))
                drawCircle(Color.Black.copy(alpha = 0.4f), 2.6f * px, Offset(cx + c * bodyR * 0.86f, cy + s * bodyR * 0.86f), style = thin)
                if (ring) drawCircle(accent.copy(alpha = 0.85f), r + 3 * px, Offset(cx, cy), style = ringStroke)
            }
        },
    )
}
