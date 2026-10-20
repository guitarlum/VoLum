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
import com.volum.design.art.smoothstep
import com.volum.design.art.withA
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * Bad Cat Mini Cat (`VoLumArtBadCat.h`, class B): the cat watches. Its gaze
 * holds and darts, it blinks slowly once in a while, the pupils widen with
 * the level and startle on a pick, eyeshine flares, and the stars twinkle.
 */
class BadCatAnimator : ArtAnimator() {
    private val rest = ArtLayer()
    private val sky = ArtLayer()
    private val eyes = ArtLayer()
    private val lid = Matrix()
    private var cx = 0f
    private var cy = 0f
    private var ew = 0f
    private var eh = 0f
    private val exc = FloatArray(2)
    private val almondX = Array(2) { FloatArray(34) }
    private val almondY = Array(2) { FloatArray(34) }
    private val starX = FloatArray(STARS)
    private val starY = FloatArray(STARS)
    private val starR = FloatArray(STARS)
    private val starRate = FloatArray(STARS)
    private val starPhase = FloatArray(STARS)
    private val starA = FloatArray(STARS)
    private val haloDots = FloatArray(3 * STARS)
    private val coreDots = FloatArray(3 * STARS)
    private val seg = FloatArray(16)
    private val gaze = FloatArray(2)
    private val g0 = FloatArray(2)
    private val g1 = FloatArray(2)
    private var flash = 0f
    private var swell = 0f
    private val white = argb(255, 240, 250, 252)
    private val rim = argb(255, 170, 235, 240)
    private val starCore = argb(255, 220, 248, 252)
    private val shineShader: Shader = ArtCanvas.addRadial(0f, 0f, 1f, withA(mix(ArtInk.Gold, ArtInk.Teal, 0.35f), 0.85f), withA(ArtInk.Teal, 0.28f))
    private val shineM = Matrix()

    override fun prepare(r: Box, d: Float) {
        if (!rest.matches(r, d)) rest.build(r, d) { SceneHeroes.badCat(it, r) }
        if (!sky.matches(r, d)) sky.build(r, d) { SceneHeroes.badCatSky(it, r) }
        if (!eyes.matches(r, d)) eyes.build(r, d) { SceneHeroes.badCatEyes(it, r, false) }
        rest.addBitmap()
        sky.addBitmap()
        eyes.addBitmap()
        val rr = min(r.w, r.h)
        cx = r.mw
        cy = r.mh
        ew = rr * 0.17f
        eh = ew * 0.5f
        val sep = rr * 0.24f
        exc[0] = cx - sep
        exc[1] = cx + sep
        for (e in 0 until 2) SceneHeroes.badCatAlmond(exc[e], cy, ew, eh, almondX[e], almondY[e])
        val f = Frand(4)
        for (i in 0 until STARS) {
            starX[i] = r.l + f.next() * r.w
            starY[i] = r.t + f.next() * r.h
            f.next()
            starR[i] = f.next() * 1f + 0.3f
            val k = i * 2654435761L.toInt()
            starRate[i] = 0.8f + 1.4f * hash01(k xor 2)
            starPhase[i] = 6.2831853f * hash01(k xor 1)
        }
    }

    override fun drop() {
        rest.reset(); sky.reset(); eyes.reset()
    }

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest) {
            g.drawLayerLit(rest, m.bloom)
            return
        }
        val wake = m.wake
        val e = m.energy
        pickResponse(m)
        val kick = max(m.pick, flash)
        val closure = wake * blinkClosure(m.clock, m.seed)
        val sy = 1f - 0.9f * closure
        val blinking = sy < 1f
        g.aboutPoint(lid, cx, cy + eh * 0.25f, 0f, 1f, sy)
        g.drawLayerLit(sky, m.bloom)
        twinkles(g, m.clock, wake * (0.3f + 0.55f * e))
        if (blinking) {
            g.drawLayerXform(eyes, lid)
            g.drawLayerXform(eyes, lid, m.bloom, additive = true)
            g.save()
            g.c.concat(lid)
        } else {
            g.drawLayerLit(eyes, m.bloom)
        }
        rims(g, min(0.9f, wake * (0.28f + 0.37f * e) + 0.55f * kick + 0.35f * closure))
        gazeAt(m.clock, m.seed)
        val reach = wake * (0.4f + 0.6f * e)
        val dx = gaze[0] * ew * 0.14f * reach
        val dy = gaze[1] * eh * 0.12f * reach
        val pw0 = ew * 0.13f
        val ph0 = eh * 0.92f
        val breathe = 1f + 0.5f * noise1(m.clock.toFloat() * 0.35f, m.seed + 40)
        val pw = (pw0 * (1f + 1.3f * e * breathe + 0.35f * m.pick + 0.7f * swell)).coerceIn(pw0, ew * 0.32f)
        val ph = ph0 * (1f - 0.08f * e)
        val haloW = pw0 * 2.4f + (pw - pw0) * 1.4f
        val haloA = min(0.7f, 0.35f * (1f + 0.8f * e))
        val fade = 1f - 0.7f * smoothstep(0.4f, 0.95f, closure)
        val shine = min(0.85f, wake * (0.3f + 0.3f * e) + 0.75f * flash)
        for (k in 0 until 2) shine(g, k, exc[k] + dx, cy + dy, shine)
        for (k in 0 until 2) {
            val px = exc[k] + dx
            val py = cy + dy
            g.fillEllipse(withA(ArtInk.Gold, haloA * fade), px - haloW, py - ph * 1.15f, px + haloW, py + ph * 1.15f)
            g.fillEllipse(withA(ArtInk.Gold, 0.95f * fade), px - pw, py - ph, px + pw, py + ph)
        }
        val flare = min(1f, 0.2f * wake * e + 0.8f * flash) * fade
        val catchW = min(1f, wake * (0.2f + 0.3f * e) + 0.9f * flash)
        val spark = ew * (0.06f + 0.22f * flash)
        for (k in 0 until 2) {
            val px = exc[k] + dx
            val py = cy + dy
            if (flare > 0.01f) g.fillEllipse(withA(ArtInk.GoldHi, 0.9f), px - pw * 0.8f, py - ph * 0.9f, px + pw * 0.8f, py + ph * 0.9f, flare, add = true)
            val lx = px - pw - ew * 0.05f
            val ly = py - ph * 0.32f
            ArtCommon.glowDotAdd(g, ArtInk.GoldHi, white, lx, ly, 0.9f + 1.1f * flash, 2.5f + 4.5f * flash, catchW)
            val o = 8 * k
            seg[o] = lx - spark; seg[o + 1] = ly; seg[o + 2] = lx + spark; seg[o + 3] = ly
            seg[o + 4] = lx; seg[o + 5] = ly - spark * 0.7f; seg[o + 6] = lx; seg[o + 7] = ly + spark * 0.7f
        }
        if (flash > 0.02f) g.batchLines(white, 1.2f, seg, 4, min(1f, flash), add = true)
        if (blinking) g.restore()
    }

    private fun pickResponse(m: ArtMotion) {
        flash = 0f
        swell = 0f
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= ArtTiming.MAX_EVENT_AGE) continue
            val tail = 1f - age / ArtTiming.MAX_EVENT_AGE
            val v = 0.7f + 0.3f * hash01((m.onsets - k) * 747796405 + m.seed)
            flash = max(flash, v * exp(-6f * age) * tail)
            swell = max(swell, v * smoothstep(0f, 0.07f, age) * exp(-2.6f * age) * tail)
        }
    }

    private fun lidAt(u: Float): Float {
        if (u <= 0f || u >= 0.7f) return 0f
        return if (u < 0.18f) smoothstep(0f, 0.18f, u) else 1f - smoothstep(0.26f, 0.7f, u)
    }

    private fun blinkClosure(clock: Double, seed: Int): Float {
        val win = floor(clock / BLINK_WINDOW)
        val n = win.toLong().toInt()
        val start = 1.2f + hash01(n * 2654435761L.toInt() xor (seed * 131 + 7)) * (BLINK_WINDOW - 3.0).toFloat()
        val u = (clock - win * BLINK_WINDOW).toFloat() - start
        var c = lidAt(u)
        if (hash01(n * 2246822519L.toInt() xor (seed * 131 + 11)) < 0.3f) c = max(c, lidAt(u - 0.8f))
        return c
    }

    private fun gazeTarget(i: Int, seed: Int, out: FloatArray) {
        val k = i * 2654435761L.toInt()
        if (hash01(k xor (seed * 977 + 1)) < 0.3f) {
            out[0] = 0f
            out[1] = 0f
            return
        }
        out[0] = 2f * hash01(k xor (seed * 977 + 3)) - 1f
        out[1] = 1.2f * hash01(k xor (seed * 977 + 5)) - 0.6f
    }

    private fun gazeAt(clock: Double, seed: Int) {
        val s = clock / GAZE_HOLD
        val n = floor(s)
        val i = n.toLong().toInt()
        gazeTarget(i - 1, seed, g0)
        gazeTarget(i, seed, g1)
        val dart = smoothstep(0f, 0.08f, (s - n).toFloat())
        val t = clock.toFloat() * 0.7f
        gaze[0] = g0[0] + (g1[0] - g0[0]) * dart + 0.12f * noise1(t, seed + 60)
        gaze[1] = g0[1] + (g1[1] - g0[1]) * dart + 0.12f * noise1(t, seed + 61)
    }

    private fun pathAlmond(g: ArtCanvas, e: Int) {
        val xs = almondX[e]
        val ys = almondY[e]
        g.moveTo(xs[0], ys[0])
        for (i in 1 until xs.size) g.lineTo(xs[i], ys[i])
        g.closePath()
    }

    private fun rims(g: ArtCanvas, glow: Float) {
        if (glow <= 0f) return
        g.clearPath()
        pathAlmond(g, 0)
        pathAlmond(g, 1)
        g.strokePath(ArtInk.Teal, 7f, 0.7f * glow, add = true, roundJoin = true)
        g.clearPath()
        pathAlmond(g, 0)
        pathAlmond(g, 1)
        g.strokePath(rim, 2.2f, glow, add = true, roundJoin = true)
    }

    private fun shine(g: ArtCanvas, e: Int, px: Float, py: Float, shine: Float) {
        if (shine <= 0f) return
        g.clearPath()
        pathAlmond(g, e)
        shineM.setScale(ew * 0.8f, ew * 0.8f)
        shineM.postTranslate(px, py)
        shineShader.setLocalMatrix(shineM)
        g.fillPath(shineShader, shine, add = true)
    }

    private fun twinkles(g: ArtCanvas, clock: Double, gain: Float) {
        if (gain <= 0f) return
        for (i in 0 until STARS) {
            val wave = sin(starRate[i].toDouble() * clock + starPhase[i].toDouble()).toFloat()
            val w2 = if (wave > 0f) wave * wave else 0f
            starA[i] = gain * w2 * w2
        }
        for (k in 0 until STAR_TIERS) {
            val lo = max(0.05f, k.toFloat() / STAR_TIERS)
            val hi = (k + 1).toFloat() / STAR_TIERS
            var n = 0
            for (i in 0 until STARS) {
                if (starA[i] <= lo || starA[i] > hi) continue
                haloDots[3 * n] = starX[i]; haloDots[3 * n + 1] = starY[i]; haloDots[3 * n + 2] = starR[i] * 2.6f + 1.2f
                coreDots[3 * n] = starX[i]; coreDots[3 * n + 1] = starY[i]; coreDots[3 * n + 2] = starR[i] * 0.9f + 0.2f
                n++
            }
            if (n == 0) continue
            g.batchDots(ArtInk.Teal, haloDots, n, 0.25f + 0.35f * hi, add = true)
            g.batchDots(starCore, coreDots, n, 0.4f + 0.55f * hi, add = true)
        }
    }

    companion object {
        const val STARS = 44
        const val STAR_TIERS = 4
        const val GAZE_HOLD = 2.6
        const val BLINK_WINDOW = 8.5
    }
}
