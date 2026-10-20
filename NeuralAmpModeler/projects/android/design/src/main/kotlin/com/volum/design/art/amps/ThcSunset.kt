package com.volum.design.art.amps

import android.graphics.Matrix
import android.graphics.Shader
import com.volum.design.art.ArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtLayer
import com.volum.design.art.ArtMotion
import com.volum.design.art.ArtTiming
import com.volum.design.art.Box
import com.volum.design.art.Frand
import com.volum.design.art.argb
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.phase
import com.volum.design.art.smoothstep
import com.volum.design.art.withA
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin

/**
 * THC Sunset (amp 14, fractal case 13): "Dark Sun". `art/VoLumArtThcSunset.h`.
 * Dark core, ring-of-fire corona, light shaft, synthwave sky over an outrun grid.
 */
object ThcSunset {
    private val sOrange = argb(255, 233, 138, 90)
    private val sMag = argb(255, 201, 74, 140)
    private val sTeal = argb(255, 120, 210, 220)
    private val sGold = argb(255, 252, 222, 145)

    class Sun(val cx: Float, val horizon: Float, val r0: Float, val cyS: Float)

    fun sunAt(r: Box): Sun {
        val horizon = r.t + r.h * 0.60f
        val r0 = min(r.w, r.h) * 0.17f
        return Sun(r.mw, horizon, r0, horizon - r0 * 2.35f)
    }

    fun sky(g: ArtCanvas, r: Box) {
        val s = sunAt(r)
        g.pathRect(r.l, r.t, r.r, s.horizon)
        // NanoVG keeps the first and last of the three desktop stops.
        g.fillPath(ArtCanvas.linear(r.l, r.t, r.l, s.horizon, withA(sTeal, 0.04f), withA(sOrange, 0.15f)))
        ArtCommon.bloom(g, s.cx, s.cyS, s.r0 * 3.2f, sOrange, 0.11f)
    }

    fun corona(g: ArtCanvas, r: Box) {
        val s = sunAt(r)
        val f = Frand(31)
        for (i in 0 until 96) {
            val a = i / 96f * 6.28318f
            val len = s.r0 * (1.02f + f.next() * 0.16f + 0.04f * sin(a * 7f))
            val ix = s.cx + cos(a) * s.r0 * 0.99f
            val iy = s.cyS + sin(a) * s.r0 * 0.99f
            val ox = s.cx + cos(a) * len
            val oy = s.cyS + sin(a) * len
            val col = if (f.next() > 0.5f) sGold else sOrange
            g.line(withA(col, 0.18f), ix, iy, ox, oy, 3.0f)
            g.line(withA(col, 0.5f + 0.45f * f.next()), ix, iy, ox, oy, 1.4f)
        }
    }

    fun core(g: ArtCanvas, r: Box) {
        val s = sunAt(r)
        val cx = s.cx
        val r0 = s.r0
        g.pathCircle(cx, s.cyS, r0)
        g.fillPath(ArtCanvas.radial(cx, s.cyS, r0, withA(sOrange, 0.16f), argb(250, 7, 8, 12)))
        g.drawCircle(withA(sGold, 0.22f), cx, s.cyS, r0, 6f)
        g.drawCircle(withA(sGold, 0.95f), cx, s.cyS, r0, 2f)
        g.drawCircle(withA(sGold, 0.40f), cx, s.cyS, r0 * 0.86f, 1f)
        val topY = s.cyS + r0 * 0.92f
        val wt = r0 * 0.20f
        val wb = r0 * 0.05f
        g.moveTo(cx - wt, topY)
        g.lineTo(cx + wt, topY)
        g.lineTo(cx + wb, s.horizon)
        g.lineTo(cx - wb, s.horizon)
        g.closePath()
        g.fillPath(ArtCanvas.linear(cx, topY, cx, s.horizon, withA(sGold, 0.34f), withA(sGold, 0f)))
        g.line(withA(sGold, 0.5f), cx - wt * 0.7f, topY, cx - wb, s.horizon, 1f)
        g.line(withA(sGold, 0.5f), cx + wt * 0.7f, topY, cx + wb, s.horizon, 1f)
        ArtCommon.dust(g, 7, cx, (topY + s.horizon) * 0.5f, wt * 1.5f, 10)
        for (k in -4..4) {
            val a = -1.5708f + k * 0.16f
            val l = r0 * (0.5f - abs(k) * 0.05f)
            g.line(withA(sGold, 0.5f), cx, s.horizon, cx + cos(a) * l, s.horizon + sin(a) * l, 1f)
        }
        g.line(withA(sOrange, 0.5f), r.l, s.horizon, r.r, s.horizon, 4f)
        g.line(withA(sGold, 0.9f), r.l, s.horizon, r.r, s.horizon, ArtInk.HeroTkThin)
    }

    fun horizontals(g: ArtCanvas, r: Box) {
        val horizon = r.t + r.h * 0.60f
        for (i in 1..8) {
            val t = i / 8f
            val yy = horizon + (r.b - horizon) * t * t
            g.line(argb((255f * (0.26f * (1f - t) + 0.08f)).toInt(), 120, 210, 220), r.l, yy, r.r, yy, 1f)
        }
    }

    fun verticals(g: ArtCanvas, r: Box) {
        val horizon = r.t + r.h * 0.60f
        for (i in -12..12) {
            val fx = r.mw + (i / 12f) * r.w * 0.55f
            g.line(argb(38, 120, 210, 220), r.mw + (i / 12f) * r.w * 0.05f, horizon, fx, r.b, 1f)
        }
    }

    fun hero(g: ArtCanvas, r: Box) {
        sky(g, r)
        corona(g, r)
        core(g, r)
        horizontals(g, r)
        verticals(g, r)
    }

    fun animator(): ArtAnimator = Animator()

    private val kThcGold = argb(255, 252, 222, 145)
    private val kThcOrange = argb(255, 233, 138, 90)
    private val kThcMag = argb(255, 201, 74, 140)
    private val kThcHot = argb(255, 255, 240, 196)

    /**
     * Class B, crossfade to live: the ring of fire turns while long corona
     * streamers wheel the other way behind it; flares circle the ring and the
     * spikes flicker and reach with the level, a pick throws the corona out and
     * lifts a prominence loop off the rim. The sun breathes, the shaft shimmers
     * and the outrun grid rolls in.
     */
    private class Animator : ArtAnimator() {
        private val rest = ArtLayer()
        private val skyL = ArtLayer()
        private val coronaL = ArtLayer()
        private val eclipse = ArtLayer()

        private val spA = FloatArray(SPIKES)
        private val spLen = FloatArray(SPIKES)
        private val spAlpha = FloatArray(SPIKES)
        private val spGold = BooleanArray(SPIKES)
        private val live = FloatArray(SPIKES * 6)
        private val rayA = FloatArray(RAYS)
        private val rayReach = FloatArray(RAYS)
        private val raySpread = FloatArray(RAYS)
        private val glow0 = FloatArray(4 * SPIKES)
        private val glow1 = FloatArray(4 * SPIKES)
        private val loop = FloatArray(2 * LOOP_PTS)
        private val praise = FloatArray(4 * 9)

        private var sun = Sun(0f, 0f, 1f, 0f)
        private var breathShader: Shader? = null
        private var shaftShader: Shader? = null
        private val streamSoft = arrayOfNulls<Shader>(BUCKETS)
        private val streamGold = arrayOfNulls<Shader>(BUCKETS)
        private val unit = Matrix()

        override fun prepare(r: Box, d: Float) {
            if (!rest.matches(r, d)) rest.build(r, d) { hero(it, r) }
            if (!skyL.matches(r, d)) skyL.build(r, d) { sky(it, r) }
            if (!coronaL.matches(r, d)) coronaL.build(r, d) { corona(it, r) }
            if (!eclipse.matches(r, d)) eclipse.build(r, d) {
                core(it, r)
                verticals(it, r)
            }
            rest.addBitmap()
            skyL.addBitmap()
            coronaL.addBitmap()
            eclipse.addBitmap()
            sun = sunAt(r)
            replaySpikes()
            placeRays()
            val bigR = sun.r0 * 2.5f
            val inner = sun.r0 * 0.97f
            breathShader = ArtCanvas.addRadial(sun.cx, sun.cyS, bigR, withA(kThcOrange, 0.7f), withA(kThcOrange, 0f), inner / bigR)
            val topY = sun.cyS + sun.r0 * 0.92f
            shaftShader = ArtCanvas.addLinear(sun.cx, topY, sun.cx, sun.horizon, withA(ArtInk.Gold, 0.55f), withA(ArtInk.Gold, 0f))
            val soft = mix(kThcOrange, kThcMag, 0.3f)
            for (k in 0 until BUCKETS) {
                // Unit-radius streamer fades: the inner stop (R0 / len) is bucketed, the outer radius set per frame.
                val inner01 = MIN_INNER + (MAX_INNER - MIN_INNER) * k / (BUCKETS - 1)
                streamSoft[k] = ArtCanvas.addRadial(0f, 0f, 1f, withA(soft, 0.5f), withA(soft, 0f), inner01)
                streamGold[k] = ArtCanvas.addRadial(0f, 0f, 1f, withA(ArtInk.Gold, 0.55f), withA(ArtInk.Gold, 0f), inner01)
            }
        }

        override fun drop() {
            rest.reset(); skyL.reset(); coronaL.reset(); eclipse.reset()
        }

        override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
            if (m.isRest) {
                g.drawLayerLit(rest, m.bloom)
                return
            }
            val wake = m.wake
            val spin = phase(m.clock, CORONA_TURN, TAU)
            g.drawLayerLit(skyL, m.bloom)
            drawRays(g, m, wake)
            g.drawLayer(coronaL, 1f - wake)
            g.drawLayerAdd(coronaL, m.bloom * (1f - wake))
            drawLiveCorona(g, m, wake, spin)
            drawBreath(g, m, wake)
            drawProminences(g, m, spin)
            g.drawLayerLit(eclipse, m.bloom)
            drawGrid(g, r, m, wake)
            drawShaft(g, m, wake)
            drawPraise(g, m, wake)
        }

        private fun replaySpikes() {
            val f = Frand(31)
            for (i in 0 until SPIKES) {
                spA[i] = i / 96f * 6.28318f
                spLen[i] = sun.r0 * (1.02f + f.next() * 0.16f + 0.04f * sin(spA[i] * 7f))
                spGold[i] = f.next() > 0.5f
                spAlpha[i] = 0.5f + 0.45f * f.next()
            }
        }

        private fun placeRays() {
            for (j in 0 until RAYS) {
                val id = j * 2654435761L.toInt()
                rayA[j] = (j + 0.6f * (hash01(id xor 0x51) - 0.5f)) * TAU.toFloat() / RAYS
                rayReach[j] = 1.5f + 0.95f * hash01(id xor 0x52)
                raySpread[j] = 0.06f + 0.05f * hash01(id xor 0x53)
            }
        }

        private fun bucket(len: Float): Int {
            val inner = (sun.r0 / len).coerceIn(MIN_INNER, MAX_INNER)
            return ((inner - MIN_INNER) / (MAX_INNER - MIN_INNER) * (BUCKETS - 1) + 0.5f).toInt()
        }

        private fun streamer(g: ArtCanvas, th: Float, spread: Float, len: Float, gold: Boolean, w: Float) {
            val r0 = sun.r0 * 0.96f
            val rm = 0.5f * (sun.r0 + len)
            val mid = th + 0.45f * CURL
            val tip = th + CURL
            g.moveTo(sun.cx + cos(th - spread) * r0, sun.cyS + sin(th - spread) * r0)
            g.lineTo(sun.cx + cos(mid - 0.4f * spread) * rm, sun.cyS + sin(mid - 0.4f * spread) * rm)
            g.lineTo(sun.cx + cos(tip) * len, sun.cyS + sin(tip) * len)
            g.lineTo(sun.cx + cos(mid + 0.4f * spread) * rm, sun.cyS + sin(mid + 0.4f * spread) * rm)
            g.lineTo(sun.cx + cos(th + spread) * r0, sun.cyS + sin(th + spread) * r0)
            g.closePath()
            val s = (if (gold) streamGold else streamSoft)[bucket(len)] ?: return
            unit.setScale(len, len)
            unit.postTranslate(sun.cx, sun.cyS)
            s.setLocalMatrix(unit)
            g.fillPath(s, w, add = true)
        }

        private fun drawRays(g: ArtCanvas, m: ArtMotion, wake: Float) {
            val amp = min(1f, wake * (0.4f + 0.6f * m.energy) + 0.35f * m.pick)
            if (amp <= 0f) return
            val spin = -phase(m.clock, RAY_TURN, TAU)
            val drift = phase(m.clock, 0.45, NOISE_PERIOD)
            val shimmer = phase(m.clock, 0.7, NOISE_PERIOD)
            val reachGain = 0.8f + 0.2f * m.energy + 0.3f * m.pick
            val floorDist = 0.96f * (sun.horizon - sun.cyS)
            for (j in 0 until RAYS) {
                val th = rayA[j] + spin
                var len = sun.r0 * rayReach[j] * reachGain * (1f + 0.35f * noise1(drift, 200 + j))
                val down = sin(th + CURL) + 0.5f * raySpread[j]
                if (down > 0.05f) len = min(len, floorDist / down)
                if (len < 1.3f * sun.r0) continue
                val w = min(1f, amp * (0.8f + 0.4f * noise1(shimmer, 300 + j)))
                streamer(g, th, raySpread[j] * (1f + 0.3f * m.pick), len, gold = false, w = w)
                streamer(g, th, raySpread[j] * 0.3f, len * 0.92f, gold = true, w = w)
            }
        }

        private fun drawLiveCorona(g: ArtCanvas, m: ArtMotion, wake: Float, spin: Float) {
            if (wake <= 0f) return
            val e = m.energy
            val p = m.pick
            val flarePh = phase(m.clock, FLARE_RATE, TAU)
            val flickX = phase(m.clock, FLICK_RATE, NOISE_PERIOD)
            val pickSeed = m.onsets * 747796405 + m.seed
            val root = sun.r0 * 0.99f
            var nGold = 0
            var nOrange = 0
            for (i in 0 until SPIKES) {
                val th = spA[i] + spin
                val cs = cos(th)
                val sn = sin(th)
                val wave = 0.5f + 0.5f * sin(3f * th + flarePh)
                val flick = noise1(flickX, i)
                val burst = 0.7f + 0.3f * hash01(i * 2654435761L.toInt() xor pickSeed)
                val gain = e * (1.3f * wave + 0.5f * flick) + 1.6f * p * burst
                val len = spLen[i] + (spLen[i] - root + 0.05f * sun.r0) * gain
                val o = i * 6
                live[o] = sun.cx + cs * root
                live[o + 1] = sun.cyS + sn * root
                live[o + 2] = sun.cx + cs * len
                live[o + 3] = sun.cyS + sn * len
                live[o + 4] = min(1f, 0.5f * e * wave * wave + 0.45f * p)
                live[o + 5] = 1f + 0.35f * e + 0.3f * e * wave + 0.3f * p
                val seg = if (spGold[i]) glow0 else glow1
                val k = 4 * (if (spGold[i]) nGold++ else nOrange++)
                seg[k] = live[o]
                seg[k + 1] = live[o + 1]
                seg[k + 2] = live[o + 2]
                seg[k + 3] = live[o + 3]
            }
            val glowA = min(1f, 0.18f * (1f + 0.5f * e + 0.6f * p)) * wake
            val glowW = 3f + 2f * e + 1.5f * p
            g.batchLines(withA(kThcGold, glowA), glowW, glow0, nGold)
            g.batchLines(withA(kThcOrange, glowA), glowW, glow1, nOrange)
            for (i in 0 until SPIKES) {
                val o = i * 6
                val heat = live[o + 4]
                val col = mix(if (spGold[i]) kThcGold else kThcOrange, kThcHot, heat)
                g.line(withA(col, min(1f, spAlpha[i] * live[o + 5]) * wake), live[o], live[o + 1], live[o + 2], live[o + 3], 1.4f + 0.5f * heat)
            }
        }

        private fun drawBreath(g: ArtCanvas, m: ArtMotion, wake: Float) {
            val pulse = 0.85f + 0.15f * sin(phase(m.clock, BREATH_RATE, TAU))
            val w = min(1f, wake * (0.55f * m.energy * pulse + 0.45f * m.pick))
            if (w <= 0f) return
            val bigR = sun.r0 * 2.5f
            val inner = sun.r0 * 0.97f
            g.strokeCircleShader(breathShader ?: return, sun.cx, sun.cyS, 0.5f * (bigR + inner), bigR - inner, w, add = true)
        }

        private fun drawProminences(g: ArtCanvas, m: ArtMotion, spin: Float) {
            val core = mix(kThcMag, kThcGold, 0.55f)
            for (k in 0 until ArtTiming.ONSET_SLOTS) {
                val age = m.onsetAge[k]
                if (age >= ArtTiming.MAX_EVENT_AGE || m.onsets <= k) continue
                val id = (m.onsets - k) * 747796405 + m.seed
                val life = 1f - age / ArtTiming.MAX_EVENT_AGE
                val fade = life * life
                val foot = TAU.toFloat() * hash01(id) + spin
                val span = 0.09f + 0.09f * hash01(id xor 0x68e31da4)
                val lean = 0.6f * (hash01(id xor 0xb5297a4d.toInt()) - 0.5f)
                val rise = sun.r0 * (0.22f + 0.2f * hash01(id xor 0x1b56c4e9)) * (0.35f + 0.65f * smoothstep(0f, 0.45f, age)) * (1f + 0.2f * age)
                for (p in 0 until LOOP_PTS) {
                    val u = p.toFloat() / (LOOP_PTS - 1)
                    val lift = max(0f, sin(u * 3.14159265f)).pow(0.7f)
                    val ang = foot + span * (2f * u - 1f) + lean * span * lift
                    val rad = sun.r0 * 0.98f + rise * lift
                    loop[2 * p] = sun.cx + cos(ang) * rad
                    loop[2 * p + 1] = sun.cyS + sin(ang) * rad
                }
                g.batchPolyline(kThcMag, 6f, loop, LOOP_PTS, 0.55f * fade, add = true)
                g.batchPolyline(core, 1.8f, loop, LOOP_PTS, 0.9f * fade, add = true)
            }
        }

        private fun gridLine(g: ArtCanvas, r: Box, t: Float, a: Float) {
            if (a <= 0f) return
            val yy = sun.horizon + (r.b - sun.horizon) * t * t
            g.line(argb((255f * a).toInt(), 120, 210, 220), r.l, yy, r.r, yy, 1f)
        }

        private fun drawGrid(g: ArtCanvas, r: Box, m: ArtMotion, wake: Float) {
            if (wake < 1f) for (i in 1..8) {
                val t = i / 8f
                gridLine(g, r, t, (0.26f * (1f - t) + 0.08f) * (1f - wake))
            }
            if (wake <= 0f) return
            val scroll = phase(m.clock, GRID_RATE, 1.0)
            for (i in 0..8) {
                val t = (i + scroll) / 8f
                if (t > 1f) break
                gridLine(g, r, t, (0.26f * (1f - t) + 0.08f) * min(1f, 8f * t) * wake)
            }
        }

        private fun drawShaft(g: ArtCanvas, m: ArtMotion, wake: Float) {
            val shimmer = 0.8f + 0.2f * sin(phase(m.clock, SHIMMER_RATE, TAU))
            val w = min(1f, wake * (0.7f * m.energy + 0.5f * m.pick) * shimmer)
            if (w <= 0f) return
            val cx = sun.cx
            val topY = sun.cyS + sun.r0 * 0.92f
            val wt = sun.r0 * 0.20f
            val wb = sun.r0 * 0.05f
            g.moveTo(cx - wt, topY)
            g.lineTo(cx + wt, topY)
            g.lineTo(cx + wb, sun.horizon)
            g.lineTo(cx - wb, sun.horizon)
            g.closePath()
            g.fillPath(shaftShader ?: return, w, add = true)
        }

        private fun drawPraise(g: ArtCanvas, m: ArtMotion, wake: Float) {
            val w = min(1f, wake * (0.3f * m.energy + 0.9f * m.pick))
            if (w <= 0f) return
            val reach = 1f + 0.2f * m.energy + 1.4f * m.pick
            var n = 0
            for (k in -4..4) {
                val a = -1.5708f + k * 0.16f
                val l = sun.r0 * (0.5f - abs(k) * 0.05f) * reach
                praise[4 * n] = sun.cx
                praise[4 * n + 1] = sun.horizon
                praise[4 * n + 2] = sun.cx + cos(a) * l
                praise[4 * n + 3] = sun.horizon + sin(a) * l
                n++
            }
            g.batchLines(withA(ArtInk.Gold, 0.8f), 1.2f, praise, n, w, add = true)
        }

        companion object {
            const val SPIKES = 96
            const val RAYS = 10
            const val LOOP_PTS = 16
            const val TAU = 6.283185307179586
            const val CORONA_TURN = TAU / 40.0
            const val RAY_TURN = TAU / 64.0
            const val FLARE_RATE = 1.7
            const val FLICK_RATE = 5.0
            const val BREATH_RATE = 1.3
            const val SHIMMER_RATE = 2.3
            const val GRID_RATE = 0.45
            const val NOISE_PERIOD = 65536.0
            const val CURL = 0.14f
            const val BUCKETS = 48
            const val MIN_INNER = 0.15f
            const val MAX_INNER = 0.8f
        }
    }
}
