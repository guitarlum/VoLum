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
import com.volum.design.art.smoothstep
import com.volum.design.art.withA
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Marshall JMP 2203 (`VoLumArtJmp2203.h`, class B): the triforce stack
 * breathes apart and sways with the level, a crest climbs it front to back,
 * and each pick sends an echo back through the layers.
 */
class Jmp2203Animator : ArtAnimator() {
    private val full = ArtLayer()
    private val bloomL = ArtLayer()
    private val crest = FloatArray(LAYERS)
    private val ring = FloatArray(LAYERS)
    private val echoU = FloatArray(ArtTiming.ONSET_SLOTS)
    private val echoFade = FloatArray(ArtTiming.ONSET_SLOTS)
    private val warm = argb(255, 255, 240, 205)
    private val heartBloom = ArtCommon.MovingBloom(ArtInk.Gold, 0.6f)
    private var cx = 0f
    private var cy2 = 0f
    private var baseS = 0f
    private var rr = 0f
    private var spread = 1f
    private var sway = 0f
    private var ox = 0f
    private var oy = 0f
    private var os = 0f

    override fun prepare(r: Box, d: Float) {
        if (!full.matches(r, d)) full.build(r, d) { SceneHeroes.jmp2203(it, r) }
        if (!bloomL.matches(r, d)) bloomL.build(r, d) { ArtCommon.bloom(it, r.mw, r.mh + r.h * 0.02f, min(r.w, r.h) * 0.5f, ArtInk.Gold, 0.10f) }
        full.addBitmap()
        bloomL.addBitmap()
        cx = r.mw
        cy2 = r.mh + r.h * 0.02f
        baseS = min(r.w * 0.34f, r.h * 0.42f)
        rr = min(r.w, r.h)
    }

    override fun drop() {
        full.reset()
        bloomL.reset()
    }

    /** `Jmp2203Depth::At`: where depth [d] sits this frame. */
    private fun at(d: Float) {
        os = baseS * (1f - d * 0.14f)
        oy = cy2 - d * rr * 0.07f * spread
        ox = cx + d * sway
    }

    private fun addTriforce(g: ArtCanvas, ccx: Float, ccy: Float, s: Float, col: Int, width: Float, amount: Float) {
        if (amount <= 0.005f) return
        val ax = ccx; val ay = ccy - s
        val bx = ccx - s * 0.866f; val by = ccy + s * 0.5f
        val cxx = ccx + s * 0.866f; val cyy = ccy + s * 0.5f
        val abx = (ax + bx) / 2f; val aby = (ay + by) / 2f
        val acx = (ax + cxx) / 2f; val acy = (ay + cyy) / 2f
        val bcx = (bx + cxx) / 2f; val bcy = (by + cyy) / 2f
        g.clearPath()
        g.pathTriangle(ax, ay, abx, aby, acx, acy)
        g.pathTriangle(abx, aby, bx, by, bcx, bcy)
        g.pathTriangle(acx, acy, bcx, bcy, cxx, cyy)
        g.strokePath(col, width, sqrt(min(1f, amount)), add = true, roundJoin = true)
    }

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest) {
            g.drawLayerLit(full, m.bloom)
            return
        }
        g.drawLayerLit(bloomL, m.bloom)
        val e = m.energy
        val p = m.pick
        val t = m.clock
        spread = 1f + e * (0.5f + 0.12f * cos(ORBIT * t).toFloat()) + 0.3f * p
        sway = rr * 0.06f * e * sin(ORBIT * t).toFloat()
        for (i in 0 until LAYERS) {
            val s = max(0f, sin(CLIMB * t - 0.9 * i).toFloat())
            crest[i] = e * s * s
            ring[i] = 0f
        }
        var nEcho = 0
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= ArtTiming.MAX_EVENT_AGE) continue
            val v = 0.8f + 0.4f * hash01((m.onsets - k) * 747796405 + m.seed)
            val fade = v * exp(-1.1f * age) * (1f - smoothstep(0.95f, 1.45f, age))
            if (fade <= 0f) continue
            val u = age / ECHO_TRAVEL
            echoU[nEcho] = u
            echoFade[nEcho] = fade
            nEcho++
            for (i in 0 until LAYERS) {
                val d = i.toFloat() / (LAYERS - 1)
                ring[i] += fade * max(0f, 1f - abs(u - d) * 4f)
            }
        }
        for (i in LAYERS - 1 downTo 0) {
            val dt = i.toFloat() / (LAYERS - 1)
            at(dt)
            val a = min(1f, (0.85f - dt * 0.5f) * (1f + 0.8f * crest[i]))
            SceneHeroes.jmpTriOutline(g, ox, oy, os, withA(mix(ArtInk.Gold, ArtInk.Teal, dt), a), if (i == 0) 2.2f else 1.2f, i == 0, 5f + 6f * p)
        }
        val lift = m.bloom * m.bloom
        for (i in 0 until LAYERS) {
            val dt = i.toFloat() / (LAYERS - 1)
            at(dt)
            val ink = mix(ArtInk.Gold, ArtInk.Teal, dt)
            val thick = if (i == 0) 2.2f else 1.2f
            val front = if (i == 0) p else 0f
            val glowAmt = min(0.8f, 0.35f * lift + 0.28f * crest[i] + 0.3f * min(1f, ring[i]) + 0.3f * front)
            addTriforce(g, ox, oy, os, ink, thick + 4.5f + 6f * front, glowAmt)
            addTriforce(g, ox, oy, os, mix(ink, warm, 0.3f), thick, min(0.9f, 0.55f * crest[i]))
        }
        for (k in 0 until nEcho) {
            at(echoU[k])
            val ink = mix(ArtInk.Gold, ArtInk.Teal, echoU[k])
            addTriforce(g, ox, oy, os, ink, 6f, 0.3f * echoFade[k])
            addTriforce(g, ox, oy, os, mix(ink, warm, 0.3f), 1.6f, 0.5f * echoFade[k])
        }
        val heart = min(0.3f, 0.08f * e + 0.14f * p)
        heartBloom.draw(g, cx, cy2, rr * (0.5f + 0.06f * p), sqrt(heart) / 0.6f)
    }

    companion object {
        const val LAYERS = 6
        const val CLIMB = 2.4
        const val ORBIT = 0.45
        const val ECHO_TRAVEL = 1.05f
    }
}
