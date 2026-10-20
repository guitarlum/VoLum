package com.volum.design.art.amps

import android.graphics.Matrix
import android.graphics.Shader
import com.volum.design.art.ArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtLayer
import com.volum.design.art.ArtMotion
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.withA
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Soldano SLO100 (`VoLumArtSoldano.h`, class B): the lighthouse turns, its
 * three beams sweep the Burning Ship sea and light the crests they cross;
 * the lantern flashes when the main beam swings toward the viewer and on a pick.
 */
class SoldanoAnimator : ArtAnimator() {
    private val sea = ArtLayer()
    private val rest = ArtLayer()
    private var bx = 0f
    private var by = 0f
    private var bR = 1f

    private var crestTheta = FloatArray(0)
    private var crestX = FloatArray(0)
    private var crestY = FloatArray(0)
    private var crestDist = FloatArray(0)
    private var crestFc = FloatArray(0)
    private var crestId = IntArray(0)

    private val litX = FloatArray(MAX_LIT)
    private val litY = FloatArray(MAX_LIT)
    private val litA = FloatArray(MAX_LIT)
    private var litN = 0
    private val dots = FloatArray(3 * MAX_LIT)

    private val beamAng = FloatArray(3)
    private val beamSpread = FloatArray(3)
    private val beamLen = FloatArray(3)
    private val beamA = floatArrayOf(0.085f, 0.038f, 0.048f)
    private val beamLight = floatArrayOf(1f, 0.6f, 0.7f)

    private val cone: Shader = ArtCanvas.linear(0f, 0f, 1f, 0f, withA(ArtInk.Gold, 1f), argb(0, 200, 165, 87))
    private val coreCone: Shader = ArtCanvas.addLinear(0f, 0f, 1f, 0f, withA(ArtInk.Gold, 1f), argb(0, 200, 165, 87))
    private val lamp = ArtCommon.MovingBloom(ArtInk.GoldHi, 1f)
    private val m = Matrix()
    private val glint = mix(ArtInk.Teal, ArtInk.GoldHi, 0.6f)
    private val hot = argb(255, 255, 246, 214)

    override fun prepare(r: Box, d: Float) {
        if (!sea.matches(r, d)) sea.build(r, d) { SceneHeroes.soldanoSea(it, r) }
        if (!rest.matches(r, d)) rest.build(r, d) { SceneHeroes.soldano(it, r) }
        sea.addBitmap()
        rest.addBitmap()
        bx = r.mw
        by = r.t + r.h * 0.44f
        bR = min(r.w, r.h)
        findCrests(r)
    }

    override fun drop() {
        sea.reset()
        rest.reset()
    }

    override fun draw(g: ArtCanvas, r: Box, mo: ArtMotion) {
        val wake = mo.wake
        if (mo.isRest || wake <= 0f) {
            g.drawLayerLit(rest, mo.bloom)
            return
        }
        g.drawLayerLit(sea, mo.bloom)
        val pi = 3.14159f
        val rotor = pi * 0.14f + ((mo.clock * TURN_PER_SECOND) % 6.283185307179586).toFloat()
        val lift = 1f + 0.9f * mo.energy + 0.6f * mo.pick
        beamAng[0] = rotor; beamSpread[0] = 0.055f; beamLen[0] = bR * 1.0f
        beamAng[1] = rotor + pi; beamSpread[1] = 0.055f; beamLen[1] = bR * 0.9f
        beamAng[2] = rotor + 0.72f * pi; beamSpread[2] = 0.05f; beamLen[2] = bR * 0.85f
        litN = 0
        for (i in 0 until 3) {
            val toward = max(0f, sin(beamAng[i]))
            beamSpread[i] *= 1f + 0.6f * toward + 0.25f * mo.pick
            beamLen[i] *= (0.74f + 0.26f * abs(cos(beamAng[i]))) * (1f + 0.12f * mo.energy)
            lightCrests(beamAng[i], beamSpread[i] * 3.2f, beamLen[i], wake * beamLight[i] * (0.55f + 0.45f * mo.energy), mo.clock)
        }
        drawGlints(g)
        if (wake < 1f) restingBeams(g, 1f - wake)
        for (i in 0 until 3) coneNormal(g, beamAng[i], beamSpread[i], beamLen[i], min(0.3f, beamA[i] * lift) * wake)
        for (i in 0 until 3) coneAdd(g, beamAng[i], beamSpread[i] * 0.35f, beamLen[i] * 0.95f, (0.5f + 0.2f * mo.pick) * wake * beamLight[i] * (0.6f + 0.4f * mo.energy))
        val facing = max(0f, sin(beamAng[0])).pow(6f)
        lampBloom(g, bR * (0.16f + 0.08f * mo.pick), 0.45f * facing + 0.5f * mo.pick + 0.15f, wake)
        ArtCommon.glowDot(g, ArtInk.Gold, ArtInk.GoldHi, bx, by, 2.2f + 1.6f * mo.pick * wake, 6f + (3f * mo.energy + 4f * mo.pick) * wake)
    }

    private fun lampBloom(g: ArtCanvas, radius: Float, peak: Float, w: Float) {
        // BloomAdd(peak, w): the shader carries alpha 1, so the peak joins the blend weight.
        lamp.draw(g, bx, by, radius, min(1f, peak) * min(w, 1f))
    }

    private fun placeCone(g: ArtCanvas, ang: Float, spread: Float, len: Float) {
        g.pathTriangle(bx, by, bx + cos(ang - spread) * len, by + sin(ang - spread) * len, bx + cos(ang + spread) * len, by + sin(ang + spread) * len)
        m.setScale(len, len)
        m.postRotate(Math.toDegrees(ang.toDouble()).toFloat())
        m.postTranslate(bx, by)
    }

    /** `DrawSoldanoBeamCone` at alpha [a]: the unit gradient placed along the beam. */
    private fun coneNormal(g: ArtCanvas, ang: Float, spread: Float, len: Float, a: Float) {
        if (a <= 0f || len <= 0f) return
        placeCone(g, ang, spread, len)
        cone.setLocalMatrix(m)
        g.fillPath(cone, a)
    }

    /** The same cone under `EBlend::Add`; [a] is the stop alpha times the blend weight. */
    private fun coneAdd(g: ArtCanvas, ang: Float, spread: Float, len: Float, a: Float) {
        if (a <= 0f || len <= 0f) return
        placeCone(g, ang, spread, len)
        coreCone.setLocalMatrix(m)
        g.fillPath(coreCone, min(a, 1f), add = true)
    }

    /** `DrawSoldanoBeams(g, r, fade, false)`: the resting beams fading out under the live ones. */
    private fun restingBeams(g: ArtCanvas, fade: Float) {
        val pi = 3.14159f
        coneNormal(g, pi * 0.14f, 0.055f, bR * 1.0f, 0.085f * fade)
        coneNormal(g, pi * 0.14f + pi, 0.055f, bR * 0.9f, 0.038f * fade)
        coneNormal(g, pi * 0.86f, 0.05f, bR * 0.85f, 0.048f * fade)
    }
    private fun findCrests(r: Box) {
        val step = 2f
        val pw = r.w * 0.82f
        val ph = r.h * 0.82f
        val pl = r.mw - pw / 2f
        val pt = r.mh - ph / 2f
        val th = ArrayList<Float>(); val xs = ArrayList<Float>(); val ys = ArrayList<Float>()
        val ds = ArrayList<Float>(); val fcs = ArrayList<Float>(); val ids = ArrayList<Int>()
        var id = 0
        var px = 0f
        while (px < pw) {
            var py = 0f
            while (py < ph) {
                val cr = -1.75 + (px / pw) * 0.15
                val ci = -0.08 + (py / ph) * 0.12
                var zr = 0.0; var zi = 0.0; var it = 0
                while (zr * zr + zi * zi < 4.0 && it < 64) { val t = zr * zr - zi * zi + cr; zi = abs(2.0 * zr * zi) + ci; zr = t; it++ }
                ++id
                if (it in 4..63) {
                    val f = it / 64f
                    val fc = f * f
                    if (fc >= 0.18f) {
                        val x = pl + px
                        val y = pt + py
                        th += atan2(y - by, x - bx); xs += x; ys += y; ds += hypot(x - bx, y - by); fcs += fc; ids += id
                    }
                }
                py += 2f * step
            }
            px += 2f * step
        }
        var keepIdx = th.indices.toList()
        if (th.size > MAX_CRESTS) {
            val keep = MAX_CRESTS.toFloat() / th.size
            keepIdx = keepIdx.filter { hash01(ids[it] * 2654435761L.toInt() xor 12) <= keep }
        }
        val order = keepIdx.sortedBy { th[it] }
        crestTheta = FloatArray(order.size) { th[order[it]] }
        crestX = FloatArray(order.size) { xs[order[it]] }
        crestY = FloatArray(order.size) { ys[order[it]] }
        crestDist = FloatArray(order.size) { ds[order[it]] }
        crestFc = FloatArray(order.size) { fcs[order[it]] }
        crestId = IntArray(order.size) { ids[order[it]] }
    }

    private fun lowerBound(v: Float): Int {
        var lo = 0
        var hi = crestTheta.size
        while (lo < hi) {
            val mid = (lo + hi) ushr 1
            if (crestTheta[mid] < v) lo = mid + 1 else hi = mid
        }
        return lo
    }

    private fun scan(lo: Float, hi: Float, axis: Float, halfWidth: Float, len: Float, gain: Float, clock: Double) {
        var i = lowerBound(lo)
        while (i < crestTheta.size && crestTheta[i] <= hi) {
            if (litN >= MAX_LIT) return
            if (crestDist[i] < len) {
                var d = crestTheta[i] - axis
                d = atan2(sin(d), cos(d))
                val across = 1f - abs(d) / halfWidth
                if (across > 0f) {
                    val twinkle = 0.7f + 0.6f * noise1(clock.toFloat() * 5f, crestId[i])
                    val a = gain * (0.35f + 0.65f * crestFc[i]) * sqrt(across) * sqrt(1f - crestDist[i] / len) * twinkle
                    if (a > 0.02f) {
                        litX[litN] = crestX[i]; litY[litN] = crestY[i]; litA[litN] = min(a, 1f); litN++
                    }
                }
            }
            i++
        }
    }

    private fun lightCrests(axis0: Float, halfWidth: Float, len: Float, gain: Float, clock: Double) {
        if (crestTheta.isEmpty() || gain <= 0f) return
        val pi = 3.14159265f
        val axis = atan2(sin(axis0), cos(axis0))
        val lo = axis - halfWidth
        val hi = axis + halfWidth
        scan(max(lo, -pi), min(hi, pi), axis, halfWidth, len, gain, clock)
        if (lo < -pi) scan(lo + 2f * pi, pi, axis, halfWidth, len, gain, clock)
        if (hi > pi) scan(-pi, hi - 2f * pi, axis, halfWidth, len, gain, clock)
    }

    private fun drawGlints(g: ArtCanvas) {
        for (k in 0 until TIERS) {
            val lo = k.toFloat() / TIERS
            val hi = (k + 1).toFloat() / TIERS
            for (pass in 0 until 2) {
                var n = 0
                for (i in 0 until litN) if (litA[i] > lo && litA[i] <= hi) {
                    dots[3 * n] = litX[i]; dots[3 * n + 1] = litY[i]; dots[3 * n + 2] = if (pass == 0) 3.8f else 1.35f
                    n++
                }
                g.batchDots(if (pass == 0) glint else hot, dots, n, if (pass == 0) 0.35f + 0.35f * hi else min(1f, 0.5f + 0.5f * hi), add = true)
            }
        }
    }

    companion object {
        const val TURN_PER_SECOND = 6.283185307179586 / 7.5
        const val MAX_CRESTS = 1800
        const val MAX_LIT = 240
        const val TIERS = 3
    }
}
