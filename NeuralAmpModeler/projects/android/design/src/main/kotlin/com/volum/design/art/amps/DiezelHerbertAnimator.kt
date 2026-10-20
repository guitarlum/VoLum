package com.volum.design.art.amps

import com.volum.design.art.ArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtLayer
import com.volum.design.art.ArtMotion
import com.volum.design.art.ArtTiming
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.smoothstep
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Diezel Herbert (`VoLumArtDiezelHerbert.h`, class B): charge climbs both
 * discharge trees from their terminals with the level, pulses run along the
 * lit channels and the fronts crackle; each pick throws a bolt out to a far
 * leaf and back, and the terminals flare.
 */
class DiezelHerbertAnimator : ArtAnimator() {
    private val full = ArtLayer()
    private val bloomL = ArtLayer()
    private val tree = ArtLayer()
    private var s = SceneHeroes.DiezelSegs(0)
    private val byDist = arrayOf(IntArray(0), IntArray(0))
    private val tips = arrayOf(IntArray(0), IntArray(0))
    private val maxDist = FloatArray(2)
    private var termX = 0f
    private val termY = FloatArray(2)
    private var tierXY = Array(3) { FloatArray(0) }
    private val tierN = IntArray(3)
    private var haloXY = FloatArray(0); private var haloN = 0
    private var pulseXY = FloatArray(0); private var pulseN = 0
    private var frontXY = FloatArray(0); private var frontN = 0
    private var sparkX = FloatArray(0); private var sparkY = FloatArray(0); private var sparkId = IntArray(0); private var sparkN = 0
    private val dots = FloatArray(3 * MAX_SPARKS)
    private var path = IntArray(0)
    private var bolt = FloatArray(0)
    private val charge = FloatArray(2)
    private val flare = FloatArray(2)
    private val white = argb(255, 235, 245, 250)
    private val arc = mix(ArtInk.Teal, argb(255, 235, 245, 250), 0.35f)
    private val termBloom = ArtCommon.MovingBloom(ArtInk.Teal, 0.5f)

    override fun prepare(r: Box, d: Float) {
        s = SceneHeroes.diezelGrow(r)
        if (!full.matches(r, d)) full.build(r, d) { SceneHeroes.diezelBloom(it, r); SceneHeroes.diezelDischarge(it, r, s) }
        if (!bloomL.matches(r, d)) bloomL.build(r, d) { SceneHeroes.diezelBloom(it, r) }
        if (!tree.matches(r, d)) tree.build(r, d) { SceneHeroes.diezelDischarge(it, r, s) }
        full.addBitmap(); bloomL.addBitmap(); tree.addBitmap()
        index(r)
    }

    override fun drop() {
        full.reset(); bloomL.reset(); tree.reset()
    }

    private fun index(r: Box) {
        termX = r.mw
        termY[0] = r.b - 16f
        termY[1] = r.t + 16f
        val n = s.n
        val hasChild = BooleanArray(n)
        val lists = arrayOf(ArrayList<Int>(), ArrayList<Int>())
        maxDist.fill(0f)
        for (i in 0 until n) {
            if (s.up[i] >= 0) hasChild[s.up[i]] = true
            lists[s.root[i]] += i
            maxDist[s.root[i]] = max(maxDist[s.root[i]], s.d1[i])
        }
        for (root in 0 until 2) {
            val order = lists[root].sortedBy { s.d0[it] }
            byDist[root] = order.toIntArray()
            val t = ArrayList<Int>()
            var farthest = -1
            for (i in order) {
                if (!hasChild[i] && s.d1[i] >= 0.55f * maxDist[root]) t += i
                if (farthest < 0 || s.d1[i] > s.d1[farthest]) farthest = i
            }
            if (t.isEmpty() && farthest >= 0) t += farthest
            tips[root] = t.toIntArray()
        }
        tierXY = Array(3) { FloatArray(4 * n) }
        haloXY = FloatArray(4 * n); pulseXY = FloatArray(4 * n); frontXY = FloatArray(4 * n)
        sparkX = FloatArray(n); sparkY = FloatArray(n); sparkId = IntArray(n)
        path = IntArray(n)
        bolt = FloatArray(2 * (n + 1))
    }

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest || s.n == 0) {
            g.drawLayerLit(full, m.bloom)
            return
        }
        val wake = m.wake
        val t = m.clock.toFloat()
        val pi = 3.14159265f
        val keep = 1f - 0.55f * m.energy
        val bloomW = m.bloom * keep
        val flashW = 0.55f * m.pick
        g.drawLayerLit(bloomL, m.bloom)
        g.drawLayer(tree, keep)
        g.drawLayerAdd(tree, sqrt(bloomW * bloomW + flashW * flashW))
        charge.fill(0f)
        val lift = wake * (0.65f + 0.35f * m.energy) * (0.9f + 0.2f * noise1(t * 13f, 50))
        if (lift > 0f) {
            tierN.fill(0); haloN = 0; pulseN = 0; frontN = 0; sparkN = 0
            val seesaw = ((m.clock * SEESAW_HZ) % 1.0).toFloat() * 2f * pi
            val pulse = ((m.clock * PULSE_HZ) % 1.0).toFloat() * 2f * pi
            for (root in 0 until 2) {
                val frac = (0.12f + 0.63f * m.energy + 0.4f * m.pick + 0.09f * m.energy * sin(seesaw + root * pi) + 0.05f * wake * noise1(t * 9f, 40 + root)).coerceIn(0f, 1f)
                charge[root] = frac
                reveal(root, frac * maxDist[root] * 1.01f, pulse)
            }
            g.batchLines(ArtInk.Teal, 5.5f, haloXY, haloN, min(1f, 0.45f * lift), add = true, buttCap = true)
            g.batchLines(arc, 1.6f, tierXY[2], tierN[2], min(1f, 0.5f * lift), add = true, buttCap = true)
            g.batchLines(arc, 2.1f, tierXY[1], tierN[1], min(1f, 0.65f * lift), add = true, buttCap = true)
            g.batchLines(arc, 3f, tierXY[0], tierN[0], min(1f, 0.9f * lift), add = true, buttCap = true)
            g.batchLines(mix(ArtInk.Teal, white, 0.6f), 2.2f, pulseXY, pulseN, min(1f, (0.3f + 0.5f * m.energy) * lift), add = true, buttCap = true)
            g.batchLines(white, 2.6f, frontXY, frontN, min(1f, 0.85f * lift), add = true, buttCap = true)
        }
        flare.fill(0f)
        bolts(g, m)
        if (lift > 0f) sparks(g, t, lift)
        terminals(g, m, wake)
    }

    private fun push(v: FloatArray, n: Int, x0: Float, y0: Float, x1: Float, y1: Float): Int {
        v[4 * n] = x0; v[4 * n + 1] = y0; v[4 * n + 2] = x1; v[4 * n + 3] = y1
        return n + 1
    }

    private fun reveal(root: Int, reach: Float, pulse: Float) {
        val frontFrom = reach - min(FRONT, 0.5f * reach)
        for (idx in byDist[root]) {
            if (s.d0[idx] >= reach) break
            var x1 = s.x1[idx]
            var y1 = s.y1[idx]
            var d1 = s.d1[idx]
            if (d1 > reach) {
                val f = (reach - s.d0[idx]) / (s.d1[idx] - s.d0[idx])
                x1 = s.x0[idx] + (s.x1[idx] - s.x0[idx]) * f
                y1 = s.y0[idx] + (s.y1[idx] - s.y0[idx]) * f
                d1 = reach
                sparkX[sparkN] = x1; sparkY[sparkN] = y1; sparkId[sparkN] = idx; sparkN++
            }
            val tr = s.tier[idx]
            tierN[tr] = push(tierXY[tr], tierN[tr], s.x0[idx], s.y0[idx], x1, y1)
            if (tr < 2) haloN = push(haloXY, haloN, s.x0[idx], s.y0[idx], x1, y1)
            if (d1 > frontFrom) frontN = push(frontXY, frontN, s.x0[idx], s.y0[idx], x1, y1)
            else if (cos(PULSE_K * 0.5f * (s.d0[idx] + d1) - pulse) > PULSE_BAND) pulseN = push(pulseXY, pulseN, s.x0[idx], s.y0[idx], x1, y1)
        }
    }

    private fun sparks(g: ArtCanvas, t: Float, lift: Float) {
        if (sparkN == 0) return
        val keepFrac = if (sparkN > MAX_SPARKS) MAX_SPARKS.toFloat() / sparkN else 1f
        val halo = mix(ArtInk.Teal, ArtInk.Blue, 0.5f)
        for (pass in 0 until 2) {
            var n = 0
            var kept = 0
            for (i in 0 until sparkN) {
                if (kept >= MAX_SPARKS) break
                if (keepFrac < 1f && hash01(sparkId[i] * 2654435761L.toInt() xor 14) >= keepFrac) continue
                kept++
                val f = 0.5f + noise1(t * 20f, sparkId[i])
                if (f < 0.3f || (pass == 1 && f <= 0.6f)) continue
                dots[3 * n] = sparkX[i]; dots[3 * n + 1] = sparkY[i]; dots[3 * n + 2] = if (pass == 0) 2.5f + 2.5f * f else 1f + 0.6f * f
                n++
            }
            g.batchDots(if (pass == 0) halo else white, dots, n, min(1f, (if (pass == 0) 0.5f else 0.95f) * lift), add = true)
        }
    }

    private fun bolts(g: ArtCanvas, m: ArtMotion) {
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val a = m.onsetAge[k]
            if (a >= ArtTiming.MAX_EVENT_AGE || m.onsets < k + 1) continue
            val id = m.onsets - k
            val hsh = id * 747796405 + m.seed
            val root = if (hash01(hsh) < 0.5f) 0 else 1
            val tp = tips[root]
            if (tp.isEmpty()) continue
            val pick = (hash01(hsh xor 0x9e3779b9.toInt()) * tp.size).toInt()
            val tip = tp[min(tp.size - 1, pick)]
            val reach = min(s.d1[tip], LEADER_SPEED * a) * (1f - smoothstep(0.3f, 1.25f, a))
            val env = (0.5f + 0.5f * exp(-4f * a)) * (0.82f + 0.36f * noise1(a * 24f, id)) * (1f - smoothstep(0.85f, 1.35f, a))
            if (reach <= 0.5f || env <= 0f) continue
            var pn = 0
            var sg = tip
            while (sg >= 0) { path[pn++] = sg; sg = s.up[sg] }
            var bn = 0
            val first = path[pn - 1]
            bolt[0] = s.x0[first]; bolt[1] = s.y0[first]; bn = 1
            for (q in pn - 1 downTo 0) {
                val i = path[q]
                if (s.d1[i] <= reach) {
                    bolt[2 * bn] = s.x1[i]; bolt[2 * bn + 1] = s.y1[i]; bn++
                    continue
                }
                val f = (reach - s.d0[i]) / (s.d1[i] - s.d0[i])
                bolt[2 * bn] = s.x0[i] + (s.x1[i] - s.x0[i]) * f; bolt[2 * bn + 1] = s.y0[i] + (s.y1[i] - s.y0[i]) * f; bn++
                break
            }
            g.batchPolyline(ArtInk.Blue, 7f, bolt, bn, min(1f, 0.5f * env), add = true)
            g.batchPolyline(arc, 3f, bolt, bn, min(1f, 0.75f * env), add = true)
            g.batchPolyline(white, 1.4f, bolt, bn, min(1f, env), add = true)
            ArtCommon.glowDotAdd(g, ArtInk.Blue, white, bolt[2 * (bn - 1)], bolt[2 * (bn - 1) + 1], 1.4f, 5f, env)
            flare[root] = max(flare[root], env * exp(-3f * a))
        }
    }

    private fun terminals(g: ArtCanvas, m: ArtMotion, wake: Float) {
        for (k in 0 until 2) {
            val ty = termY[k]
            val glow = min(1f, wake * (0.35f + 0.65f * charge[k]) + flare[k])
            termBloom.draw(g, termX, ty, 16f + 18f * m.energy + 22f * m.pick + 30f * flare[k], glow)
            ArtCommon.glowDotAdd(g, ArtInk.Teal, white, termX, ty, 2.6f + 3f * m.pick + 1.5f * flare[k], 5f + 4f * m.energy, min(1f, wake + flare[k]))
            val goldW = min(1f, 0.85f * (0.4f + 0.6f * m.energy) * max(wake, flare[k]))
            if (goldW > 0f) g.fillCircle(ArtInk.GoldHi, termX, ty, 1.2f + 0.8f * m.pick, goldW, add = true)
        }
    }

    companion object {
        const val SEESAW_HZ = 0.32
        const val PULSE_HZ = 1.4
        const val PULSE_K = 6.2831853f / 64f
        const val PULSE_BAND = 0.55f
        const val FRONT = 24f
        const val LEADER_SPEED = 3200f
        const val MAX_SPARKS = 48
    }
}
